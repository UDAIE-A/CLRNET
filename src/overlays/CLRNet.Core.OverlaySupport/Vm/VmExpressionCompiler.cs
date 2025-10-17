using System;
using System.Collections.Generic;
using System.Linq;
using System.Linq.Expressions;
using System.Reflection;
using System.Reflection.Emit;
using System.Runtime.InteropServices;
using System.Security.Cryptography;

namespace CLRNet.Runtime.Dynamic
{
    /// <summary>
    /// Provides a best-effort bridge from Expression.Compile to the Track B IL virtual machine. Complex expressions fall back to
    /// the built-in interpreter, but simple arithmetic delegates execute through CLRNET.VM for consistency with dynamic IL.
    /// </summary>
    internal static class VmExpressionCompiler
    {
        private static readonly HashSet<short> SupportedOpcodes = new HashSet<short>
        {
            OpCodes.Nop.Value,
            OpCodes.Ldarg_0.Value,
            OpCodes.Ldarg_1.Value,
            OpCodes.Ldarg_2.Value,
            OpCodes.Ldarg_3.Value,
            OpCodes.Ldarg_S.Value,
            OpCodes.Ldarg.Value,
            OpCodes.Ldc_I4.Value,
            OpCodes.Ldc_I4_0.Value,
            OpCodes.Ldc_I4_1.Value,
            OpCodes.Ldc_I4_2.Value,
            OpCodes.Ldc_I4_3.Value,
            OpCodes.Ldc_I4_4.Value,
            OpCodes.Ldc_I4_5.Value,
            OpCodes.Ldc_I4_6.Value,
            OpCodes.Ldc_I4_7.Value,
            OpCodes.Ldc_I4_8.Value,
            OpCodes.Ldc_I4_M1.Value,
            OpCodes.Ldc_I4_S.Value,
            OpCodes.Add.Value,
            OpCodes.Sub.Value,
            OpCodes.Mul.Value,
            OpCodes.Div.Value,
            OpCodes.Ret.Value
        };

        private static readonly OpCode[] SingleByteOpCodes = BuildSingleByteOpCodes();
        private static readonly OpCode[] MultiByteOpCodes = BuildMultiByteOpCodes();

        public static Delegate Compile(LambdaExpression expression)
        {
            if (expression == null)
            {
                throw new ArgumentNullException("expression");
            }

            if (TryCompileInternal(expression, out Delegate result))
            {
                return result;
            }

            return expression.Compile(preferInterpretation: true);
        }

        public static TDelegate Compile<TDelegate>(Expression<TDelegate> expression)
        {
            if (expression == null)
            {
                throw new ArgumentNullException("expression");
            }

            if (TryCompileInternal(expression, out Delegate result))
            {
                return (TDelegate)(object)result;
            }

            return expression.Compile(preferInterpretation: true);
        }

        private static bool TryCompileInternal(LambdaExpression expression, out Delegate result)
        {
            result = null;
            try
            {
                var baked = VmBackedLambda.TryCreate(expression);
                if (baked == null)
                {
                    return false;
                }

                result = baked.CreateDelegate(expression.Type);
                return true;
            }
            catch (DllNotFoundException)
            {
                return false;
            }
            catch (EntryPointNotFoundException)
            {
                return false;
            }
            catch (NotSupportedException)
            {
                return false;
            }
            catch (InvalidOperationException)
            {
                return false;
            }
        }

        private sealed class VmBackedLambda : IDisposable
        {
            private readonly VmProgramHandle _program;
            private readonly Type[] _parameterTypes;
            private readonly Type _returnType;
            private readonly int _localCount;

            private VmBackedLambda(VmProgramHandle program, Type[] parameterTypes, Type returnType, int localCount)
            {
                _program = program;
                _parameterTypes = parameterTypes;
                _returnType = returnType;
                _localCount = localCount;
            }

            public static VmBackedLambda TryCreate(LambdaExpression expression)
            {
                MethodInfo method = BakeMethod(expression);
                MethodBody body = method.GetMethodBody();
                if (body == null)
                {
                    return null;
                }

                byte[] il = body.GetILAsByteArray();
                ValidateSupportedInstructions(il);

                byte[] compiled = BuildFatMethod(il, body);
                string cacheKey = ComputeCacheKey(compiled);

                VmProgramHandle program = ClrNetVmNative.Compile(compiled, cacheKey);
                return new VmBackedLambda(program, GetParameterTypes(method), method.ReturnType, body.LocalVariables.Count);
            }

            public Delegate CreateDelegate(Type delegateType)
            {
                MethodInfo invoke = delegateType.GetMethod("Invoke");
                if (invoke == null)
                {
                    throw new InvalidOperationException("Delegate type is missing Invoke method.");
                }

            var parameters = invoke.GetParameters();
            if (parameters.Length != _parameterTypes.Length)
            {
                throw new InvalidOperationException("Delegate signature mismatch for VM-backed expression.");
            }

            var parameterExpressions = new ParameterExpression[parameters.Length];
            for (int i = 0; i < parameters.Length; i++)
            {
                parameterExpressions[i] = Expression.Parameter(parameters[i].ParameterType, parameters[i].Name);
            }

            MethodInfo invokeHelper = typeof(VmBackedLambda).GetMethod(nameof(InvokeInternal), BindingFlags.Instance | BindingFlags.NonPublic);
            var call = Expression.Call(Expression.Constant(this), invokeHelper, Expression.NewArrayInit(typeof(object),
                parameterExpressions.Select(p => Expression.Convert(p, typeof(object)))));

            Expression body = BuildReturnExpression(call, invoke.ReturnType);
            return Expression.Lambda(delegateType, body, parameterExpressions).Compile();
        }

        private Expression BuildReturnExpression(Expression call, Type returnType)
        {
            if (returnType == typeof(void))
            {
                return Expression.Block(call, Expression.Empty());
            }

            return Expression.Convert(call, returnType);
        }

            private object InvokeInternal(object[] arguments)
            {
                if (arguments == null)
                {
                    throw new ArgumentNullException("arguments");
                }

                if (arguments.Length != _parameterTypes.Length)
                {
                    throw new ArgumentException("Argument count mismatch.", "arguments");
                }

                VmValue[] argumentBuffer = new VmValue[_parameterTypes.Length];
                for (int i = 0; i < _parameterTypes.Length; i++)
                {
                    argumentBuffer[i] = VmValue.FromManaged(arguments[i], _parameterTypes[i]);
                }

                VmValue[] localBuffer = _localCount > 0 ? new VmValue[_localCount] : Array.Empty<VmValue>();

                using (var pinnedArgs = new PinnedArray<VmValue>(argumentBuffer))
                using (var pinnedLocals = new PinnedArray<VmValue>(localBuffer))
                {
                    var context = new VmExecutionContextNative
                    {
                        arguments = pinnedArgs.Pointer,
                        argumentCount = (uint)pinnedArgs.Length,
                        locals = pinnedLocals.Pointer,
                        localCount = (uint)pinnedLocals.Length,
                        timeBudgetTicks = 10_000,
                        memoryBudgetBytes = (UIntPtr)65536,
                        sandboxNamespace = IntPtr.Zero,
                        userData = IntPtr.Zero
                    };

                    var result = new VmExecutionResultNative();
                    int hr = ClrNetVmNative.Execute(_program, ref context, ref result);
                    if (hr != 0)
                    {
                        throw new InvalidOperationException("CLRNet_VM_Execute failed with HRESULT 0x" + hr.ToString("X8"));
                    }

                    if (result.success == 0)
                    {
                        string failure = result.failureReason != IntPtr.Zero ? Marshal.PtrToStringUni(result.failureReason) : "unknown";
                        throw new InvalidOperationException("VM execution failed: " + failure);
                    }

                    return VmValue.ToManaged(result.returnValue, _returnType);
                }
            }

            public void Dispose()
            {
                _program.Dispose();
            }
        }

        private sealed class PinnedArray<T> : IDisposable where T : struct
        {
            private readonly GCHandle _handle;

            public PinnedArray(T[] array)
            {
                Array = array ?? throw new ArgumentNullException("array");
                if (array.Length > 0)
                {
                    _handle = GCHandle.Alloc(array, GCHandleType.Pinned);
                }
            }

            public T[] Array { get; }

            public IntPtr Pointer => Array.Length == 0 ? IntPtr.Zero : _handle.AddrOfPinnedObject();

            public int Length => Array.Length;

            public void Dispose()
            {
                if (_handle.IsAllocated)
                {
                    _handle.Free();
                }
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct VmExecutionContextNative
        {
            public IntPtr arguments;
            public uint argumentCount;
            public IntPtr locals;
            public uint localCount;
            public ulong timeBudgetTicks;
            public UIntPtr memoryBudgetBytes;
            public IntPtr sandboxNamespace;
            public IntPtr userData;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct VmExecutionResultNative
        {
            public int success;
            public uint stepsExecuted;
            public IntPtr returnValue;
            public IntPtr failureReason;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct VmValue
        {
            public VmValueKind Kind;
            public int Padding0;
            public long Data;

            public static VmValue FromManaged(object value, Type targetType)
            {
                VmValue result = default(VmValue);
                if (value == null)
                {
                    if (targetType.IsValueType)
                    {
                        value = Activator.CreateInstance(targetType);
                    }
                    else
                    {
                        throw new NotSupportedException("Reference types are not supported by the VM expression pipeline.");
                    }
                }

                if (targetType == typeof(int) || targetType == typeof(uint) || targetType == typeof(short) || targetType == typeof(byte) || targetType == typeof(sbyte))
                {
                    result.Kind = VmValueKind.Int32;
                    result.Data = Convert.ToInt32(value);
                    return result;
                }

                if (targetType == typeof(bool))
                {
                    result.Kind = VmValueKind.Int32;
                    result.Data = Convert.ToBoolean(value) ? 1 : 0;
                    return result;
                }

                if (targetType == typeof(long) || targetType == typeof(ulong))
                {
                    result.Kind = VmValueKind.Int64;
                    result.Data = Convert.ToInt64(value);
                    return result;
                }

                throw new NotSupportedException("Only primitive integer expressions are currently supported by the VM pipeline.");
            }

            public static object ToManaged(IntPtr pointer, Type returnType)
            {
                if (returnType == typeof(void))
                {
                    return null;
                }

                if (returnType == typeof(int) || returnType == typeof(uint) || returnType == typeof(short) || returnType == typeof(byte) || returnType == typeof(sbyte))
                {
                    return pointer.ToInt32();
                }

                if (returnType == typeof(bool))
                {
                    return pointer != IntPtr.Zero;
                }

                if (returnType == typeof(long) || returnType == typeof(ulong))
                {
                    return pointer.ToInt64();
                }

                throw new NotSupportedException("Only primitive integer expressions are currently supported by the VM pipeline.");
            }
        }

        private enum VmValueKind
        {
            Uninitialized,
            Int32,
            Int64,
            Float,
            Double,
            Object,
            ManagedPointer,
            Null
        }

        private static class ClrNetVmNative
        {
            private const string DllName = "clrnet-vm";

            [DllImport(DllName, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi)]
            private static extern int CLRNet_VM_CompileIL(byte[] ilCode, int ilSize, string cacheKey, out IntPtr outHandle);

            [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
            private static extern int CLRNet_VM_Execute(IntPtr handle, ref VmExecutionContextNative context, ref VmExecutionResultNative result);

            [DllImport(DllName, CallingConvention = CallingConvention.StdCall)]
            private static extern int CLRNet_VM_Release(IntPtr handle);

            public static VmProgramHandle Compile(byte[] il, string cacheKey)
            {
                int hr = CLRNet_VM_CompileIL(il, il.Length, cacheKey, out IntPtr handle);
                if (hr != 0 || handle == IntPtr.Zero)
                {
                    throw new InvalidOperationException("CLRNet_VM_CompileIL failed with HRESULT 0x" + hr.ToString("X8"));
                }

                return new VmProgramHandle(handle);
            }

            public static int Execute(VmProgramHandle handle, ref VmExecutionContextNative context, ref VmExecutionResultNative result)
            {
                return CLRNet_VM_Execute(handle.DangerousGetHandle(), ref context, ref result);
            }

            public static void Release(IntPtr handle)
            {
                if (handle != IntPtr.Zero)
                {
                    CLRNet_VM_Release(handle);
                }
            }
        }

        private sealed class VmProgramHandle : SafeHandle
        {
            public VmProgramHandle() : base(IntPtr.Zero, true)
            {
            }

            public VmProgramHandle(IntPtr existing) : this()
            {
                SetHandle(existing);
            }

            public override bool IsInvalid => handle == IntPtr.Zero;

            protected override bool ReleaseHandle()
            {
                ClrNetVmNative.Release(handle);
                return true;
            }
        }

        private static MethodInfo BakeMethod(LambdaExpression expression)
        {
            var assembly = AssemblyBuilder.DefineDynamicAssembly(new AssemblyName("CLRNet.Vm.Expressions"), AssemblyBuilderAccess.Run);
            ModuleBuilder module = assembly.DefineDynamicModule("CLRNet.Vm.Expressions.Module");
            TypeBuilder type = module.DefineType("VmLambda" + Guid.NewGuid().ToString("N"), TypeAttributes.Public | TypeAttributes.Sealed | TypeAttributes.Abstract);
            Type[] parameterTypes = GetParameterTypes(expression);
            MethodBuilder method = type.DefineMethod("Invoke", MethodAttributes.Public | MethodAttributes.Static, expression.ReturnType, parameterTypes);
            try
            {
                expression.CompileToMethod(method);
            }
            catch (InvalidOperationException)
            {
                throw new NotSupportedException("Expression tree cannot be compiled into a standalone method.");
            }

            Type created = type.CreateType();
            MethodInfo invoke = created.GetMethod("Invoke");
            if (invoke == null)
            {
                throw new NotSupportedException("Generated method is missing Invoke implementation.");
            }

            return invoke;
        }

        private static Type[] GetParameterTypes(LambdaExpression expression)
        {
            Type[] result = new Type[expression.Parameters.Count];
            for (int i = 0; i < expression.Parameters.Count; i++)
            {
                result[i] = expression.Parameters[i].Type;
            }
            return result;
        }

        private static Type[] GetParameterTypes(MethodInfo method)
        {
            ParameterInfo[] parameters = method.GetParameters();
            Type[] result = new Type[parameters.Length];
            for (int i = 0; i < parameters.Length; i++)
            {
                result[i] = parameters[i].ParameterType;
            }
            return result;
        }

        private static byte[] BuildFatMethod(byte[] il, MethodBody body)
        {
            const ushort CorILMethodFatFormat = 0x0003;
            const ushort CorILMethodInitLocals = 0x0010;
            ushort flags = CorILMethodFatFormat;
            if (body.InitLocals)
            {
                flags |= CorILMethodInitLocals;
            }

            ushort headerSizeDWords = 3;
            uint header0 = (uint)(flags | (headerSizeDWords << 12) | (body.MaxStackSize << 16));
            byte[] result = new byte[12 + il.Length];
            Buffer.BlockCopy(BitConverter.GetBytes(header0), 0, result, 0, 4);
            Buffer.BlockCopy(BitConverter.GetBytes(il.Length), 0, result, 4, 4);
            Buffer.BlockCopy(BitConverter.GetBytes(body.LocalSignatureMetadataToken), 0, result, 8, 4);
            Buffer.BlockCopy(il, 0, result, 12, il.Length);
            return result;
        }

        private static void ValidateSupportedInstructions(byte[] il)
        {
            int offset = 0;
            while (offset < il.Length)
            {
                OpCode opcode = ReadOpCode(il, ref offset);
                if (!SupportedOpcodes.Contains(opcode.Value))
                {
                    throw new NotSupportedException("Opcode " + opcode + " is not supported by the VM expression pipeline.");
                }

                offset += GetOperandSize(opcode.OperandType);
            }
        }

        private static OpCode ReadOpCode(byte[] il, ref int offset)
        {
            byte code = il[offset++];
            if (code == 0xFE)
            {
                return MultiByteOpCodes[il[offset++]];
            }

            return SingleByteOpCodes[code];
        }

        private static int GetOperandSize(OperandType operandType)
        {
            switch (operandType)
            {
                case OperandType.InlineBrTarget:
                case OperandType.InlineField:
                case OperandType.InlineI:
                case OperandType.InlineMethod:
                case OperandType.InlineSig:
                case OperandType.InlineString:
                case OperandType.InlineTok:
                case OperandType.InlineType:
                case OperandType.ShortInlineR:
                    return 4;
                case OperandType.InlineI8:
                case OperandType.InlineR:
                    return 8;
                case OperandType.ShortInlineBrTarget:
                case OperandType.ShortInlineI:
                case OperandType.ShortInlineVar:
                    return 1;
                case OperandType.InlineSwitch:
                    throw new NotSupportedException("Switch instructions are not supported in VM expressions.");
                case OperandType.InlineNone:
                    return 0;
                default:
                    return 0;
            }
        }

        private static string ComputeCacheKey(byte[] il)
        {
            using (var sha1 = SHA1.Create())
            {
                byte[] hash = sha1.ComputeHash(il);
                return BitConverter.ToString(hash).Replace("-", string.Empty).ToLowerInvariant();
            }
        }

        private static OpCode[] BuildSingleByteOpCodes()
        {
            var result = new OpCode[0x100];
            foreach (FieldInfo field in typeof(OpCodes).GetFields(BindingFlags.Public | BindingFlags.Static))
            {
                if (field.FieldType != typeof(OpCode))
                {
                    continue;
                }

                OpCode value = (OpCode)field.GetValue(null);
                ushort code = (ushort)value.Value;
                if (code < 0x100)
                {
                    result[code] = value;
                }
            }

            return result;
        }

        private static OpCode[] BuildMultiByteOpCodes()
        {
            var result = new OpCode[0x100];
            foreach (FieldInfo field in typeof(OpCodes).GetFields(BindingFlags.Public | BindingFlags.Static))
            {
                if (field.FieldType != typeof(OpCode))
                {
                    continue;
                }

                OpCode value = (OpCode)field.GetValue(null);
                ushort code = (ushort)value.Value;
                if (code >= 0x100)
                {
                    result[code & 0xFF] = value;
                }
            }

            return result;
        }
    }
}
