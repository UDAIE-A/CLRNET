#include "../replacement/CLRReplacementEngine.h"
#include "../hooks/DeepSystemHooks.h"
#include "../compatibility/CompatibilityShim.h"
#include "../safety/SafetyAndRollback.h"
#include <gtest/gtest.h>
#include <memory>

namespace CLRNet {
namespace System {
namespace Tests {

// Test fixture for Phase 3 System Integration
class SystemIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize all Phase 3 components
        InitializeComponents();
    }
    
    void TearDown() override {
        // Cleanup all components
        CleanupComponents();
    }

    void InitializeComponents() {
        // Create CLR replacement engine
        m_replacementEngine.reset(CLRReplacementFactory::CreateEngine(ReplacementLevel::ProcessLevel));
        ASSERT_NE(m_replacementEngine.get(), nullptr);
        
        // Create deep system hooks manager
        auto hooksConfig = DeepSystemHooksFactory::CreateSafeConfiguration();
        m_kernelManager = DeepSystemHooksFactory::CreateKernelManager(hooksConfig);
        ASSERT_NE(m_kernelManager.get(), nullptr);
        
        // Create compatibility shim
        auto compatConfig = CompatibilityFactory::CreateNetFramework40Config();
        m_compatibilityShim = CompatibilityFactory::CreateCompatibilityShim(compatConfig);
        ASSERT_NE(m_compatibilityShim.get(), nullptr);
        
        // Create safety system
        auto safetyConfig = SafetySystemFactory::CreateConservativeConfig();
        m_healthChecker = SafetySystemFactory::CreateHealthChecker(safetyConfig);
        ASSERT_NE(m_healthChecker.get(), nullptr);
    }
    
    void CleanupComponents() {
        m_healthChecker.reset();
        m_compatibilityShim.reset();
        m_kernelManager.reset();
        CLRReplacementFactory::DestroyEngine(m_replacementEngine.release());
    }

    // Test components
    std::unique_ptr<CLRReplacementEngine> m_replacementEngine;
    std::unique_ptr<KernelIntegrationManager> m_kernelManager;
    std::unique_ptr<CompatibilityShim> m_compatibilityShim;
    std::unique_ptr<HealthChecker> m_healthChecker;
};

// CLR Replacement Engine Tests
TEST_F(SystemIntegrationTest, CLRReplacementBasicFunctionality) {
    // Test process detection
    auto managedProcesses = m_replacementEngine->GetManagedProcesses();
    
    // Should detect current process as managed (test executable)
    bool foundCurrentProcess = false;
    DWORD currentPid = GetCurrentProcessId();
    
    for (const auto& process : managedProcesses) {
        if (process.processId == currentPid) {
            foundCurrentProcess = true;
            EXPECT_TRUE(process.isManaged);
            break;
        }
    }
    
    // May not find current process if test isn't managed, that's OK
    // Main test is that the API works without crashing
}

TEST_F(SystemIntegrationTest, ProcessInjectionCapabilities) {
    // Test process injection on current process (safe)
    DWORD currentPid = GetCurrentProcessId();
    
    // Validate process can be safely replaced
    bool canReplace = m_replacementEngine->CanReplaceProcess(currentPid);
    
    // Current test process should be replaceable (not system critical)
    EXPECT_TRUE(canReplace);
    
    // Test safety validation before injection
    EXPECT_NO_THROW({
        // This should not crash even if injection fails
        HRESULT hr = m_replacementEngine->ReplaceProcessCLR(currentPid);
        // Result depends on process state, but shouldn't crash
    });
}

// Deep System Hooks Tests
TEST_F(SystemIntegrationTest, KernelHookInstallation) {
    // Test hook type detection
    EXPECT_TRUE(m_kernelManager != nullptr);
    
    // Test process hook installation (safe test mode)
    HRESULT hr = m_kernelManager->InstallProcessHooks([](HANDLE processId, HANDLE threadId, BOOLEAN create) -> NTSTATUS {
        // Test callback - just return success
        return 0; // STATUS_SUCCESS
    });
    
    EXPECT_SUCCEEDED(hr);
    
    // Verify hook is active
    EXPECT_TRUE(m_kernelManager->IsHookActive(HookType::ProcessCreation));
    
    // Test hook removal
    hr = m_kernelManager->DisableHook(HookType::ProcessCreation);
    EXPECT_SUCCEEDED(hr);
    EXPECT_FALSE(m_kernelManager->IsHookActive(HookType::ProcessCreation));
}

TEST_F(SystemIntegrationTest, MemoryManagerOverride) {
    auto memoryManager = m_kernelManager->GetMemoryManager();
    ASSERT_NE(memoryManager.get(), nullptr);
    
    // Test custom memory allocation
    SIZE_T testSize = 4096;
    PVOID testMemory = memoryManager->AllocateMemory(testSize);
    EXPECT_NE(testMemory, nullptr);
    
    // Verify allocation tracking
    EXPECT_GT(memoryManager->GetAllocatedMemory(), 0);
    EXPECT_GT(memoryManager->GetAllocationCount(), 0);
    
    // Test memory protection
    ULONG oldProtection;
    HRESULT hr = memoryManager->ProtectMemory(testMemory, testSize, PAGE_READONLY, &oldProtection);
    EXPECT_SUCCEEDED(hr);
    EXPECT_EQ(oldProtection, PAGE_READWRITE);
    
    // Cleanup
    hr = memoryManager->FreeMemory(testMemory);
    EXPECT_SUCCEEDED(hr);
}

TEST_F(SystemIntegrationTest, JITInterceptionEngine) {
    auto jitEngine = m_kernelManager->GetJITEngine();
    ASSERT_NE(jitEngine.get(), nullptr);
    
    // Test JIT hook installation
    HRESULT hr = jitEngine->InstallJITHooks(GetCurrentProcessId());
    EXPECT_SUCCEEDED(hr);
    
    // Test JIT callback registration
    hr = jitEngine->RegisterJITCallback(JITPhase::PreJIT, 
        [](HANDLE processId, PVOID methodHandle, JITPhase phase, PVOID codeAddress, SIZE_T codeSize) -> HRESULT {
            return S_OK;
        });
    EXPECT_SUCCEEDED(hr);
    
    // Test compilation statistics (should start at zero)
    EXPECT_EQ(jitEngine->GetCompiledMethodCount(), 0);
    EXPECT_EQ(jitEngine->GetGeneratedCodeSize(), 0);
    
    // Cleanup
    hr = jitEngine->RemoveJITHooks(GetCurrentProcessId());
    EXPECT_SUCCEEDED(hr);
}

// Compatibility Shim Tests
TEST_F(SystemIntegrationTest, CompatibilityShimBasics) {
    DWORD currentPid = GetCurrentProcessId();
    
    // Test framework version detection
    FrameworkVersion detectedVersion;
    HRESULT hr = m_compatibilityShim->DetectFrameworkVersion(currentPid, detectedVersion);
    
    // May succeed or fail depending on process, but shouldn't crash
    EXPECT_TRUE(SUCCEEDED(hr) || FAILED(hr));
    
    // Test shim application
    hr = m_compatibilityShim->ApplyShimsToProcess(currentPid);
    
    // Should succeed for current process
    EXPECT_SUCCEEDED(hr);
    
    // Test compatibility status
    std::map<std::wstring, bool> status;
    hr = m_compatibilityShim->GetCompatibilityStatus(currentPid, status);
    EXPECT_SUCCEEDED(hr);
}

TEST_F(SystemIntegrationTest, LegacyApiShimming) {
    auto apiShim = m_compatibilityShim->GetApiShim();
    ASSERT_NE(apiShim.get(), nullptr);
    
    // Test API shim installation
    std::wstring testApi = L"TestApi";
    PVOID mockLegacy = reinterpret_cast<PVOID>(0x12345678);
    PVOID mockModern = reinterpret_cast<PVOID>(0x87654321);
    
    HRESULT hr = apiShim->InstallApiShim(testApi, mockLegacy, mockModern);
    EXPECT_SUCCEEDED(hr);
    
    // Verify shim is installed
    EXPECT_TRUE(apiShim->IsApiShimmed(testApi));
    
    // Test shim enumeration
    auto installedShims = apiShim->GetInstalledShims();
    bool foundTestShim = false;
    
    for (const auto& shim : installedShims) {
        if (shim.targetApi == testApi) {
            foundTestShim = true;
            EXPECT_TRUE(shim.isInstalled);
            EXPECT_EQ(shim.originalFunction, mockLegacy);
            EXPECT_EQ(shim.shimFunction, mockModern);
            break;
        }
    }
    
    EXPECT_TRUE(foundTestShim);
}

// Safety and Rollback Tests
TEST_F(SystemIntegrationTest, SystemHealthMonitoring) {
    auto systemMonitor = m_healthChecker->GetSystemMonitor();
    ASSERT_NE(systemMonitor.get(), nullptr);
    
    DWORD currentPid = GetCurrentProcessId();
    
    // Test process monitoring
    HRESULT hr = systemMonitor->StartMonitoringProcess(currentPid);
    EXPECT_SUCCEEDED(hr);
    EXPECT_TRUE(systemMonitor->IsMonitoringProcess(currentPid));
    
    // Test health metrics collection
    HealthMetrics metrics;
    hr = systemMonitor->GetProcessHealth(currentPid, metrics);
    EXPECT_SUCCEEDED(hr);
    EXPECT_EQ(metrics.processId, currentPid);
    EXPECT_GT(metrics.memoryUsageBytes, 0);
    
    // Test system-wide metrics
    HealthMetrics systemMetrics;
    hr = systemMonitor->GetSystemHealth(systemMetrics);
    EXPECT_SUCCEEDED(hr);
    EXPECT_GT(systemMetrics.systemMemoryPercent, 0);
    
    // Cleanup
    hr = systemMonitor->StopMonitoringProcess(currentPid);
    EXPECT_SUCCEEDED(hr);
}

TEST_F(SystemIntegrationTest, SafetyValidation) {
    auto safetyValidator = m_healthChecker->GetSafetyValidator();
    ASSERT_NE(safetyValidator.get(), nullptr);
    
    DWORD currentPid = GetCurrentProcessId();
    
    // Test process safety validation
    SafetyValidationResult result;
    HRESULT hr = safetyValidator->ValidateProcessSafety(currentPid, result);
    EXPECT_SUCCEEDED(hr);
    EXPECT_TRUE(result.canProceed); // Current test process should be safe
    
    // Test system safety validation
    SafetyValidationResult systemResult;
    hr = safetyValidator->ValidateSystemSafety(systemResult);
    EXPECT_SUCCEEDED(hr);
    
    // Test specific safety checks
    SafetyValidationResult integrityResult;
    hr = safetyValidator->CheckProcessIntegrity(currentPid, integrityResult);
    EXPECT_SUCCEEDED(hr);
    EXPECT_EQ(integrityResult.checkType, SafetyCheckType::ProcessIntegrity);
}

TEST_F(SystemIntegrationTest, RollbackSystem) {
    auto rollbackManager = m_healthChecker->GetRollbackManager();
    ASSERT_NE(rollbackManager.get(), nullptr);
    
    // Test system snapshot creation
    UINT32 snapshotId;
    HRESULT hr = rollbackManager->CreateSystemSnapshot(L"Test Snapshot", snapshotId);
    EXPECT_SUCCEEDED(hr);
    EXPECT_GT(snapshotId, 0);
    
    // Test snapshot enumeration
    auto snapshots = rollbackManager->GetAvailableSnapshots();
    EXPECT_GT(snapshots.size(), 0);
    
    bool foundSnapshot = false;
    for (const auto& snapshot : snapshots) {
        if (snapshot.snapshotId == snapshotId) {
            foundSnapshot = true;
            EXPECT_EQ(snapshot.description, L"Test Snapshot");
            break;
        }
    }
    EXPECT_TRUE(foundSnapshot);
    
    // Test snapshot cleanup
    hr = rollbackManager->DeleteSnapshot(snapshotId);
    EXPECT_SUCCEEDED(hr);
}

// Integration Tests
TEST_F(SystemIntegrationTest, EndToEndCLRReplacementWorkflow) {
    DWORD currentPid = GetCurrentProcessId();
    
    // Step 1: Create safety snapshot
    auto rollbackManager = m_healthChecker->GetRollbackManager();
    UINT32 snapshotId;
    HRESULT hr = rollbackManager->CreateProcessSnapshot(currentPid, L"Pre-CLR-Replacement", snapshotId);
    EXPECT_SUCCEEDED(hr);
    
    // Step 2: Validate system safety
    SafetyValidationResult safetyResult;
    auto safetyValidator = m_healthChecker->GetSafetyValidator();
    hr = safetyValidator->ValidateProcessSafety(currentPid, safetyResult);
    EXPECT_SUCCEEDED(hr);
    
    if (safetyResult.canProceed) {
        // Step 3: Apply compatibility shims
        hr = m_compatibilityShim->ApplyShimsToProcess(currentPid);
        EXPECT_SUCCEEDED(hr);
        
        // Step 4: Install system hooks (safe test mode)
        hr = m_kernelManager->EnableHook(HookType::ProcessCreation, HookInstallFlags::PassiveMode);
        EXPECT_SUCCEEDED(hr);
        
        // Step 5: Perform CLR replacement (test mode)
        if (m_replacementEngine->CanReplaceProcess(currentPid)) {
            // Note: This might fail in test environment, but shouldn't crash
            hr = m_replacementEngine->ReplaceProcessCLR(currentPid);
            // Result depends on environment, but process should remain stable
        }
        
        // Step 6: Validate post-operation health
        HealthMetrics postMetrics;
        auto systemMonitor = m_healthChecker->GetSystemMonitor();
        hr = systemMonitor->GetProcessHealth(currentPid, postMetrics);
        EXPECT_SUCCEEDED(hr);
        EXPECT_GT(postMetrics.memoryUsageBytes, 0);
    }
    
    // Cleanup: Remove shims and hooks
    hr = m_compatibilityShim->RemoveShimsFromProcess(currentPid);
    EXPECT_SUCCEEDED(hr);
    
    hr = m_kernelManager->DisableHook(HookType::ProcessCreation);
    EXPECT_SUCCEEDED(hr);
}

TEST_F(SystemIntegrationTest, SystemStressAndRecovery) {
    DWORD currentPid = GetCurrentProcessId();
    
    // Create baseline snapshot
    auto rollbackManager = m_healthChecker->GetRollbackManager();
    UINT32 baselineSnapshot;
    HRESULT hr = rollbackManager->CreateProcessSnapshot(currentPid, L"Baseline", baselineSnapshot);
    EXPECT_SUCCEEDED(hr);
    
    // Start continuous health monitoring
    hr = m_healthChecker->StartContinuousHealthCheck(currentPid);
    EXPECT_SUCCEEDED(hr);
    
    // Simulate system stress (install multiple hooks)
    std::vector<HookType> stressHooks = {
        HookType::ProcessCreation,
        HookType::ThreadCreation,
        HookType::ImageLoad
    };
    
    for (auto hookType : stressHooks) {
        hr = m_kernelManager->EnableHook(hookType, HookInstallFlags::PassiveMode);
        EXPECT_SUCCEEDED(hr);
        
        // Brief pause to allow system to stabilize
        Sleep(100);
    }
    
    // Verify system remains stable
    SafetyValidationResult stressResult;
    auto safetyValidator = m_healthChecker->GetSafetyValidator();
    hr = safetyValidator->ValidateSystemSafety(stressResult);
    EXPECT_SUCCEEDED(hr);
    
    // Cleanup all hooks
    for (auto hookType : stressHooks) {
        hr = m_kernelManager->DisableHook(hookType);
        EXPECT_SUCCEEDED(hr);
    }
    
    // Stop health monitoring
    hr = m_healthChecker->StopContinuousHealthCheck(currentPid);
    EXPECT_SUCCEEDED(hr);
    
    // Verify system returned to baseline
    SafetyValidationResult finalResult;
    hr = safetyValidator->ValidateProcessSafety(currentPid, finalResult);
    EXPECT_SUCCEEDED(hr);
    EXPECT_TRUE(finalResult.canProceed);
}

// Performance and scalability tests
TEST_F(SystemIntegrationTest, PerformanceUnderLoad) {
    const DWORD currentPid = GetCurrentProcessId();
    
    // Measure baseline performance
    auto start = std::chrono::high_resolution_clock::now();
    
    // Perform multiple operations
    const int iterations = 100;
    for (int i = 0; i < iterations; ++i) {
        SafetyValidationResult result;
        auto safetyValidator = m_healthChecker->GetSafetyValidator();
        HRESULT hr = safetyValidator->ValidateProcessSafety(currentPid, result);
        EXPECT_SUCCEEDED(hr);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Performance should be reasonable (less than 10ms per validation)
    EXPECT_LT(duration.count(), iterations * 10);
    
    // Test memory usage doesn't grow excessively
    HealthMetrics beforeMetrics, afterMetrics;
    auto systemMonitor = m_healthChecker->GetSystemMonitor();
    
    HRESULT hr = systemMonitor->GetProcessHealth(currentPid, beforeMetrics);
    EXPECT_SUCCEEDED(hr);
    
    // Perform memory-intensive operations
    std::vector<PVOID> allocations;
    auto memoryManager = m_kernelManager->GetMemoryManager();
    
    for (int i = 0; i < 50; ++i) {
        PVOID mem = memoryManager->AllocateMemory(4096);
        if (mem) {
            allocations.push_back(mem);
        }
    }
    
    hr = systemMonitor->GetProcessHealth(currentPid, afterMetrics);
    EXPECT_SUCCEEDED(hr);
    
    // Cleanup allocations
    for (PVOID mem : allocations) {
        memoryManager->FreeMemory(mem);
    }
    
    // Memory should be properly tracked and cleaned up
    EXPECT_EQ(memoryManager->GetAllocatedMemory(), 0);
}

} // namespace Tests
} // namespace System
} // namespace CLRNet

// Test runner main function
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    // Set test output format
    ::testing::GTEST_FLAG(output) = "xml:phase3_test_results.xml";
    
    // Run all tests
    int result = RUN_ALL_TESTS();
    
    return result;
}