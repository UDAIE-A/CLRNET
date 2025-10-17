using System;
using System.Collections.Generic;
using System.CommandLine;
using System.IO;
using System.Linq;
using Mono.Cecil;
using Mono.Cecil.Cil;
using Mono.Cecil.Rocks;

namespace CLRNet.ModernCSharpPipeline;

internal static class Program
{
    private static readonly string[] DefaultStripAttributes = new[]
    {
        "System.Runtime.CompilerServices.SkipLocalsInitAttribute",
        "System.Runtime.CompilerServices.RequiredMemberAttribute",
        "System.Runtime.CompilerServices.CompilerGeneratedAttribute",
        "System.Diagnostics.CodeAnalysis.DynamicallyAccessedMembersAttribute",
        "System.Runtime.CompilerServices.RefSafetyRulesAttribute"
    };

    public static int Main(string[] args)
    {
        var inputOption = new Option<FileInfo>("--input", "Input assembly to post-process")
        {
            IsRequired = true
        };
        var outputOption = new Option<FileInfo?>("--output", "Optional output path; defaults to overwriting the input assembly");
        var embedJsonOption = new Option<string[]>("--embed-json", () => Array.Empty<string>(),
            "Embed JSON resource stubs using NAME=PATH or NAME:PATH syntax");
        var stripAttributeOption = new Option<string[]>("--strip-attribute", () => Array.Empty<string>(),
            "Fully qualified attribute names to remove. Defaults to known unsupported metadata.");
        var verboseOption = new Option<bool>("--verbose", "Emit detailed rewrite information");

        var root = new RootCommand("CLRNET modern C# IL post-pass")
        {
            inputOption,
            outputOption,
            embedJsonOption,
            stripAttributeOption,
            verboseOption
        };

        root.SetHandler((FileInfo input, FileInfo? output, string[] embeds, string[] strip, bool verbose) =>
        {
            return Run(input, output, embeds, strip, verbose);
        }, inputOption, outputOption, embedJsonOption, stripAttributeOption, verboseOption);

        return root.Invoke(args);
    }

    private static int Run(FileInfo input, FileInfo? output, string[] embeds, string[] strip, bool verbose)
    {
        if (!input.Exists)
        {
            throw new FileNotFoundException($"Input assembly '{input.FullName}' not found.");
        }

        var outputPath = output?.FullName ?? input.FullName;
        var stripSet = new HashSet<string>(DefaultStripAttributes, StringComparer.Ordinal);
        foreach (var attribute in strip)
        {
            if (!string.IsNullOrWhiteSpace(attribute))
            {
                stripSet.Add(attribute.Trim());
            }
        }

        var embedEntries = ParseEmbedEntries(embeds);

        using var assembly = AssemblyDefinition.ReadAssembly(input.FullName, new ReaderParameters
        {
            ReadWrite = false,
            InMemory = true
        });

        bool modified = false;
        var overlayAssembly = EnsureAssemblyReference(assembly.MainModule, "CLRNet.Core.OverlaySupport");
        var facadeRuntime = EnsureAssemblyReference(assembly.MainModule, "CLRNet.Facade.System.Runtime");
        var facadeTasks = EnsureAssemblyReference(assembly.MainModule, "CLRNet.Facade.System.Threading.Tasks.Extensions");
        var facadeJson = EnsureAssemblyReference(assembly.MainModule, "CLRNet.Facade.System.Text.Json");
        var facadeBuffers = EnsureAssemblyReference(assembly.MainModule, "CLRNet.Facade.System.Buffers");
        var facadeIo = EnsureAssemblyReference(assembly.MainModule, "CLRNet.Facade.System.IO");
        var facadeHttp = EnsureAssemblyReference(assembly.MainModule, "CLRNet.Facade.System.Net.Http");

        modified |= RedirectInstructions(assembly.MainModule, overlayAssembly, verbose);
        modified |= RetargetTypeReferences(assembly.MainModule, overlayAssembly, facadeRuntime, facadeTasks, facadeJson, facadeBuffers, facadeIo, facadeHttp, verbose);
        modified |= StripAttributes(assembly.MainModule, stripSet, verbose);
        modified |= EmbedJsonResources(assembly.MainModule, embedEntries, verbose);

        if (!modified)
        {
            if (!string.Equals(input.FullName, outputPath, StringComparison.OrdinalIgnoreCase))
            {
                File.Copy(input.FullName, outputPath, overwrite: true);
            }
            if (verbose)
            {
                Console.WriteLine("No changes were necessary during IL post-processing.");
            }
            return 0;
        }

        Directory.CreateDirectory(Path.GetDirectoryName(outputPath)!);
        assembly.Write(outputPath);
        if (verbose)
        {
            Console.WriteLine($"Wrote rewritten assembly to '{outputPath}'.");
        }
        return 0;
    }

    private static Dictionary<string, string> ParseEmbedEntries(IEnumerable<string> entries)
    {
        var result = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        foreach (var entry in entries)
        {
            if (string.IsNullOrWhiteSpace(entry))
            {
                continue;
            }

            var separatorIndex = entry.IndexOfAny(new[] { '=', ':' });
            string name;
            string path;
            if (separatorIndex < 0)
            {
                path = entry.Trim();
                name = Path.GetFileName(path);
            }
            else
            {
                name = entry.Substring(0, separatorIndex).Trim();
                path = entry[(separatorIndex + 1)..].Trim();
            }

            if (string.IsNullOrEmpty(name))
            {
                name = Path.GetFileName(path);
            }

            if (string.IsNullOrEmpty(path))
            {
                continue;
            }

            result[name] = Path.GetFullPath(path);
        }

        return result;
    }

    private static AssemblyNameReference EnsureAssemblyReference(ModuleDefinition module, string name)
    {
        var existing = module.AssemblyReferences.FirstOrDefault(r => string.Equals(r.Name, name, StringComparison.Ordinal));
        if (existing != null)
        {
            return existing;
        }

        var reference = new AssemblyNameReference(name, new Version(1, 0, 0, 0));
        module.AssemblyReferences.Add(reference);
        return reference;
    }

    private static bool RedirectInstructions(ModuleDefinition module, AssemblyNameReference overlayAssembly, bool verbose)
    {
        bool modified = false;
        foreach (var type in module.GetTypes())
        {
            foreach (var method in type.Methods)
            {
                if (!method.HasBody)
                {
                    continue;
                }

                var processor = method.Body.GetILProcessor();
                for (int i = 0; i < method.Body.Instructions.Count; i++)
                {
                    var instruction = method.Body.Instructions[i];
                    if (instruction.Operand is MethodReference methodReference)
                    {
                        if (TryRedirectExpressionCompile(module, overlayAssembly, instruction, methodReference, verbose))
                        {
                            modified = true;
                            continue;
                        }

                        if (TryRedirectJson(module, overlayAssembly, instruction, methodReference, verbose))
                        {
                            modified = true;
                            continue;
                        }

                        if (TryRedirectTasks(module, overlayAssembly, instruction, methodReference, verbose))
                        {
                            modified = true;
                            continue;
                        }
                    }

                    if (instruction.OpCode == OpCodes.Newobj && instruction.Operand is MethodReference ctor &&
                        string.Equals(ctor.DeclaringType.FullName, "System.Net.Http.HttpClientHandler", StringComparison.Ordinal))
                    {
                        var safeType = new TypeReference("System.Net.Http", "SafeHttpClientHandler", module, overlayAssembly);
                        var safeCtor = new MethodReference(".ctor", module.TypeSystem.Void, safeType)
                        {
                            HasThis = true,
                            ExplicitThis = false
                        };

                        instruction.Operand = module.ImportReference(safeCtor);
                        instruction.OpCode = OpCodes.Newobj;
                        modified = true;
                        if (verbose)
                        {
                            Console.WriteLine($"\tRedirected new HttpClientHandler() -> SafeHttpClientHandler in {method.FullName}.");
                        }
                    }
                }

                method.Body.OptimizeMacros();
            }
        }

        return modified;
    }

    private static bool TryRedirectExpressionCompile(ModuleDefinition module, AssemblyNameReference overlayAssembly, Instruction instruction, MethodReference methodReference, bool verbose)
    {
        if (!string.Equals(methodReference.Name, "Compile", StringComparison.Ordinal) || methodReference.Parameters.Count != 0)
        {
            return false;
        }

        if (methodReference.DeclaringType.FullName == "System.Linq.Expressions.LambdaExpression")
        {
            var compilerType = new TypeReference("System.Linq.Expressions", "CLRNetExpressionCompiler", module, overlayAssembly);
            var newMethod = new MethodReference("Compile", module.ImportReference(methodReference.ReturnType), compilerType)
            {
                HasThis = false,
                ExplicitThis = false,
                CallingConvention = MethodCallingConvention.Default
            };
            newMethod.Parameters.Add(new ParameterDefinition(module.ImportReference(methodReference.DeclaringType)));
            instruction.OpCode = OpCodes.Call;
            instruction.Operand = module.ImportReference(newMethod);
            if (verbose)
            {
                Console.WriteLine("\tPatched LambdaExpression.Compile() to CLRNetExpressionCompiler.");
            }
            return true;
        }

        if (methodReference.DeclaringType.Namespace == "System.Linq.Expressions" && methodReference.DeclaringType.Name == "Expression`1")
        {
            var compilerType = new TypeReference("System.Linq.Expressions", "CLRNetExpressionCompiler", module, overlayAssembly);
            var newMethod = new MethodReference("Compile", module.ImportReference(methodReference.ReturnType), compilerType)
            {
                HasThis = false,
                ExplicitThis = false,
                CallingConvention = MethodCallingConvention.Generic
            };

            foreach (var parameter in methodReference.DeclaringType.GenericParameters)
            {
                var generic = new GenericParameter(parameter.Name, newMethod);
                newMethod.GenericParameters.Add(generic);
            }

            newMethod.Parameters.Add(new ParameterDefinition(module.ImportReference(methodReference.DeclaringType)));
            MethodReference imported = module.ImportReference(newMethod);
            if (methodReference is GenericInstanceMethod genericInstance)
            {
                var newGeneric = new GenericInstanceMethod(imported);
                foreach (var argument in genericInstance.GenericArguments)
                {
                    newGeneric.GenericArguments.Add(module.ImportReference(argument));
                }
                instruction.Operand = newGeneric;
            }
            else
            {
                instruction.Operand = imported;
            }

            instruction.OpCode = OpCodes.Call;
            if (verbose)
            {
                Console.WriteLine("\tPatched Expression<T>.Compile() to CLRNetExpressionCompiler.");
            }
            return true;
        }

        return false;
    }

    private static bool TryRedirectJson(ModuleDefinition module, AssemblyNameReference overlayAssembly, Instruction instruction, MethodReference methodReference, bool verbose)
    {
        if (!string.Equals(methodReference.DeclaringType.Namespace, "System.Text.Json", StringComparison.Ordinal))
        {
            return false;
        }

        if (methodReference.DeclaringType.Scope == overlayAssembly)
        {
            return false;
        }

        methodReference.DeclaringType.Scope = overlayAssembly;
        if (methodReference is GenericInstanceMethod generic)
        {
            generic.ElementMethod.DeclaringType.Scope = overlayAssembly;
        }

        if (verbose)
        {
            Console.WriteLine($"\tRetargeted System.Text.Json call to CLRNet overlay in {methodReference.FullName}.");
        }
        return true;
    }

    private static bool TryRedirectTasks(ModuleDefinition module, AssemblyNameReference overlayAssembly, Instruction instruction, MethodReference methodReference, bool verbose)
    {
        if (!string.Equals(methodReference.DeclaringType.Namespace, "System.Threading.Tasks", StringComparison.Ordinal))
        {
            return false;
        }

        if (!methodReference.DeclaringType.Name.StartsWith("ValueTask", StringComparison.Ordinal))
        {
            return false;
        }

        if (methodReference.DeclaringType.Scope == overlayAssembly)
        {
            return false;
        }

        methodReference.DeclaringType.Scope = overlayAssembly;
        if (methodReference is GenericInstanceMethod generic)
        {
            generic.ElementMethod.DeclaringType.Scope = overlayAssembly;
        }

        if (verbose)
        {
            Console.WriteLine($"\tRetargeted ValueTask call to CLRNet overlay in {methodReference.FullName}.");
        }
        return true;
    }

    private static bool RetargetTypeReferences(
        ModuleDefinition module,
        AssemblyNameReference overlay,
        AssemblyNameReference facadeRuntime,
        AssemblyNameReference facadeTasks,
        AssemblyNameReference facadeJson,
        AssemblyNameReference facadeBuffers,
        AssemblyNameReference facadeIo,
        AssemblyNameReference facadeHttp,
        bool verbose)
    {
        bool modified = false;
        foreach (var typeReference in module.GetTypeReferences())
        {
            if (TypeRedirects.TryGetValue(typeReference.FullName, out var target))
            {
                var desired = target switch
                {
                    RedirectTarget.Overlay => overlay,
                    RedirectTarget.FacadeRuntime => facadeRuntime,
                    RedirectTarget.FacadeTasks => facadeTasks,
                    RedirectTarget.FacadeJson => facadeJson,
                    RedirectTarget.FacadeBuffers => facadeBuffers,
                    RedirectTarget.FacadeIo => facadeIo,
                    RedirectTarget.FacadeHttp => facadeHttp,
                    _ => null
                };

                if (desired != null && typeReference.Scope != desired)
                {
                    typeReference.Scope = desired;
                    modified = true;
                    if (verbose)
                    {
                        Console.WriteLine($"\tRetargeted {typeReference.FullName} -> {desired.Name}.");
                    }
                }
            }
        }

        return modified;
    }

    private static bool StripAttributes(ModuleDefinition module, HashSet<string> stripSet, bool verbose)
    {
        bool modified = false;

        foreach (var type in module.GetTypes())
        {
            modified |= StripAttributes(type.CustomAttributes, stripSet, verbose, $"type {type.FullName}");

            foreach (var field in type.Fields)
            {
                modified |= StripAttributes(field.CustomAttributes, stripSet, verbose, $"field {field.FullName}");
            }

            foreach (var method in type.Methods)
            {
                modified |= StripAttributes(method.CustomAttributes, stripSet, verbose, $"method {method.FullName}");
                if (method.HasParameters)
                {
                    foreach (var parameter in method.Parameters)
                    {
                        modified |= StripAttributes(parameter.CustomAttributes, stripSet, verbose, $"parameter {parameter.Name} on {method.FullName}");
                    }
                }
            }
        }

        return modified;
    }

    private static bool StripAttributes(Mono.Collections.Generic.Collection<CustomAttribute> attributes, HashSet<string> stripSet, bool verbose, string owner)
    {
        bool modified = false;
        for (int i = attributes.Count - 1; i >= 0; i--)
        {
            var attribute = attributes[i];
            if (stripSet.Contains(attribute.AttributeType.FullName))
            {
                attributes.RemoveAt(i);
                modified = true;
                if (verbose)
                {
                    Console.WriteLine($"\tRemoved {attribute.AttributeType.FullName} from {owner}.");
                }
            }
        }

        return modified;
    }

    private static bool EmbedJsonResources(ModuleDefinition module, Dictionary<string, string> resources, bool verbose)
    {
        bool modified = false;
        foreach (var entry in resources)
        {
            if (!File.Exists(entry.Value))
            {
                throw new FileNotFoundException($"Embedded JSON file '{entry.Value}' not found.");
            }

            byte[] payload = File.ReadAllBytes(entry.Value);
            var resource = new EmbeddedResource(entry.Key, ManifestResourceAttributes.Public, payload);

            var existing = module.Resources.FirstOrDefault(r => string.Equals(r.Name, entry.Key, StringComparison.OrdinalIgnoreCase));
            if (existing != null)
            {
                module.Resources.Remove(existing);
            }

            module.Resources.Add(resource);
            modified = true;
            if (verbose)
            {
                Console.WriteLine($"\tEmbedded resource '{entry.Key}' from '{entry.Value}'.");
            }
        }

        return modified;
    }

    private enum RedirectTarget
    {
        Overlay,
        FacadeRuntime,
        FacadeTasks,
        FacadeJson,
        FacadeBuffers,
        FacadeIo,
        FacadeHttp
    }

    private static readonly Dictionary<string, RedirectTarget> TypeRedirects = new(StringComparer.Ordinal)
    {
        ["System.Threading.Tasks.ValueTask"] = RedirectTarget.Overlay,
        ["System.Threading.Tasks.ValueTask`1"] = RedirectTarget.Overlay,
        ["System.Threading.Tasks.ValueTask/ValueTaskAwaiter"] = RedirectTarget.Overlay,
        ["System.Threading.Tasks.ValueTask`1/ValueTaskAwaiter"] = RedirectTarget.Overlay,
        ["System.Net.Http.SafeHttpClientHandler"] = RedirectTarget.Overlay,
        ["System.Text.Json.JsonSerializer"] = RedirectTarget.Overlay,
        ["System.Text.Json.JsonSerializerOptions"] = RedirectTarget.Overlay,
        ["System.Text.Json.Utf8JsonWriter"] = RedirectTarget.Overlay,
        ["System.Buffers.ArrayPool`1"] = RedirectTarget.Overlay,
        ["System.Buffers.ArrayBufferWriter`1"] = RedirectTarget.Overlay,
        ["System.IO.WinRtStorageExtensions"] = RedirectTarget.Overlay,
        ["System.Threading.Tasks.ValueTaskSourceStatus"] = RedirectTarget.FacadeRuntime,
        ["System.Runtime.CompilerServices.AsyncValueTaskMethodBuilder"] = RedirectTarget.FacadeRuntime,
        ["System.Runtime.CompilerServices.AsyncValueTaskMethodBuilder`1"] = RedirectTarget.FacadeRuntime,
        ["System.Net.Http.HttpClient"] = RedirectTarget.FacadeHttp,
        ["System.IO.RandomAccess"] = RedirectTarget.FacadeIo
    };
}
