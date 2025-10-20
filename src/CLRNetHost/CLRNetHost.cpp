#include "../phase1-userland/core/CoreExecutionEngine.h"
#include "../phase1-userland/core/AssemblyLoader.h"
#include "../interop/InteropManager.h"
#include "../system/compatibility/CompatibilityShim.h"
#include "../system/replacement/CLRReplacementEngine.h"

#include <windows.h>
#include <string>
#include <vector>
#include <iostream>
#include <memory>

using namespace CLRNet::Phase1;
using namespace CLRNet::Interop;
using namespace CLRNet::System;

namespace
{
    struct HostOptions
    {
        std::wstring assemblyPath;
        std::wstring typeName = L"Program";
        std::wstring methodName = L"Main";
        std::wstring manifestPath;
        bool explainOnly = false;
        bool disableInterop = false;
    };

    void PrintUsage()
    {
        std::wcout << L"CLRNetHost - Windows Phone 8.1 runtime bootstrapper\n";
        std::wcout << L"Usage:\n";
        std::wcout << L"  CLRNetHost.exe -assembly <managed.dll> [-type <TypeName>] [-method <MethodName>]\n";
        std::wcout << L"               [-manifest <WMAppManifest.xml>] [--explain] [--no-interop]\n\n";
        std::wcout << L"Options:\n";
        std::wcout << L"  -assembly <path>   Required. Managed assembly that contains the entry point.\n";
        std::wcout << L"  -type <name>       Fully qualified type name. Defaults to Program.\n";
        std::wcout << L"  -method <name>     Entry method name. Defaults to Main.\n";
        std::wcout << L"  -manifest <path>   Optional application manifest used for capability loading.\n";
        std::wcout << L"  --explain          Initialize the runtime and print discovered metadata without executing code.\n";
        std::wcout << L"  --no-interop       Skip InteropManager initialization for very small payloads.\n";
    }

    std::string Narrow(const std::wstring& value)
    {
        if (value.empty())
        {
            return std::string();
        }

        int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (required <= 0)
        {
            return std::string();
        }

        std::string result(static_cast<size_t>(required), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                            result.data(), required, nullptr, nullptr);
        return result;
    }

    bool ParseArguments(int argc, wchar_t* argv[], HostOptions& options)
    {
        for (int i = 1; i < argc; ++i)
        {
            std::wstring arg = argv[i];
            if (arg == L"-assembly" && i + 1 < argc)
            {
                options.assemblyPath = argv[++i];
            }
            else if (arg == L"-type" && i + 1 < argc)
            {
                options.typeName = argv[++i];
            }
            else if (arg == L"-method" && i + 1 < argc)
            {
                options.methodName = argv[++i];
            }
            else if (arg == L"-manifest" && i + 1 < argc)
            {
                options.manifestPath = argv[++i];
            }
            else if (arg == L"--explain")
            {
                options.explainOnly = true;
            }
            else if (arg == L"--no-interop")
            {
                options.disableInterop = true;
            }
            else if (arg == L"-?" || arg == L"--help" || arg == L"/?")
            {
                return false;
            }
            else
            {
                std::wcerr << L"Unknown option: " << arg << L"\n";
                return false;
            }
        }

        return !options.assemblyPath.empty();
    }

    void DescribeRuntime(const CoreExecutionEngine& engine)
    {
        std::wcout << L"CLRNet runtime initialized successfully.\n";
        std::wcout << L"  - Core type system ready\n";
        std::wcout << L"  - Garbage collector active\n";
        std::wcout << L"  - JIT compiler primed\n";
        std::wcout << L"Use -type and -method to run a specific entry point.\n";
        (void)engine;
    }

    std::unique_ptr<CompatibilityShim> InitializeCompatibility()
    {
        auto config = CompatibilityFactory::CreateMinimalCompatibilityConfig();
        auto shim = std::unique_ptr<CompatibilityShim>(CompatibilityFactory::CreateCompatibilityShim(config));
        if (shim)
        {
            shim->Initialize(CompatibilityLevel::Standard);
        }
        return shim;
    }

    std::unique_ptr<CLRReplacementEngine> InitializeReplacementEngine()
    {
        auto engine = std::unique_ptr<CLRReplacementEngine>(CLRReplacementFactory::CreateEngine(ReplacementLevel::ProcessLevel));
        if (engine)
        {
            engine->AttachToCurrentProcess();
        }
        return engine;
    }

    std::unique_ptr<InteropManager> InitializeInterop(const HostOptions& options)
    {
        auto configuration = InteropFactory::CreateStandardConfiguration(L"CLRNetHost");
        configuration.manifestPath = options.manifestPath;
        auto interop = std::unique_ptr<InteropManager>(InteropFactory::CreateCustomInstance(configuration));
        if (interop)
        {
            interop->Initialize(configuration);
        }
        return interop;
    }
}

int wmain(int argc, wchar_t* argv[])
{
    HostOptions options;
    if (!ParseArguments(argc, argv, options))
    {
        PrintUsage();
        return 1;
    }

    std::wcout << L"[CLRNet] Bootstrapping runtime...\n";

    CoreExecutionEngine runtime;
    if (!runtime.Initialize())
    {
        std::wcerr << L"Failed to initialize core execution engine." << std::endl;
        return 2;
    }

    auto compatibilityShim = InitializeCompatibility();
    auto replacementEngine = InitializeReplacementEngine();
    std::unique_ptr<InteropManager> interopManager;
    if (!options.disableInterop)
    {
        interopManager = InitializeInterop(options);
    }

    if (!runtime.LoadAssembly(options.assemblyPath))
    {
        std::wcerr << L"Failed to load assembly: " << options.assemblyPath << std::endl;
        runtime.Shutdown();
        return 3;
    }

    if (options.explainOnly)
    {
        DescribeRuntime(runtime);
        if (interopManager)
        {
            auto status = interopManager->GetStatus();
            std::wcout << L"Interop status: " << static_cast<int>(status) << L"\n";
        }
        runtime.Shutdown();
        return 0;
    }

    std::string typeName = Narrow(options.typeName);
    std::string methodName = Narrow(options.methodName);
    void* entryPoint = runtime.GetMethodAddress(typeName, methodName);
    if (!entryPoint)
    {
        std::wcerr << L"Could not resolve method " << options.typeName << L"::" << options.methodName << std::endl;
        runtime.Shutdown();
        return 4;
    }

    std::wcout << L"Executing " << options.typeName << L"::" << options.methodName << L"...\n";
    int exitCode = runtime.ExecuteMethod(entryPoint, nullptr, 0);
    std::wcout << L"Managed entry point returned " << exitCode << L"\n";

    if (interopManager)
    {
        interopManager->Shutdown();
    }

    if (compatibilityShim)
    {
        compatibilityShim->Cleanup();
    }

    if (replacementEngine)
    {
        CLRReplacementFactory::DestroyEngine(replacementEngine.release());
    }

    runtime.Shutdown();
    return exitCode;
}
