#include "../../src/phase1-userland/core/CoreExecutionEngine.h"
#include "../../src/phase1-userland/core/AssemblyLoader.h"
#include "../../src/interop/InteropManager.h"
#include "../../src/system/compatibility/CompatibilityShim.h"
#include "../../src/system/replacement/CLRReplacementEngine.h"

#include <windows.h>
#include <iostream>
#include <memory>

using namespace CLRNet::Phase1;
using namespace CLRNet::Interop;
using namespace CLRNet::System;

int wmain()
{
    std::wcout << L"[CLRNetTests] Starting smoke tests...\n";

    CoreExecutionEngine runtime;
    if (!runtime.Initialize())
    {
        std::wcerr << L"Failed to initialize CoreExecutionEngine" << std::endl;
        return 1;
    }

    runtime.Shutdown();

    auto config = CompatibilityFactory::CreateMinimalCompatibilityConfig();
    auto shim = std::unique_ptr<CompatibilityShim>(CompatibilityFactory::CreateCompatibilityShim(config));
    if (shim)
    {
        shim->Initialize(CompatibilityLevel::Minimal);
        shim->Cleanup();
    }

    auto engine = std::unique_ptr<CLRReplacementEngine>(CLRReplacementFactory::CreateEngine(ReplacementLevel::ProcessLevel));
    if (engine)
    {
        engine->AttachToCurrentProcess();
        CLRReplacementFactory::DestroyEngine(engine.release());
    }

    auto interopConfig = InteropFactory::CreateMinimalConfiguration(L"CLRNetTests");
    auto interop = std::unique_ptr<InteropManager>(InteropFactory::CreateCustomInstance(interopConfig));
    if (interop)
    {
        interop->Initialize(interopConfig);
        interop->Shutdown();
    }

    std::wcout << L"[CLRNetTests] All smoke tests completed successfully." << std::endl;
    return 0;
}
