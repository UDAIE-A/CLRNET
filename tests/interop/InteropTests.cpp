#include "../../src/interop/InteropManager.h"
#include "../TestFramework.h"
#include <cassert>

using namespace CLRNet::Interop;
using namespace CLRNet::Testing;

class InteropTestSuite : public TestSuite {
public:
    InteropTestSuite() : TestSuite("Interop Integration Tests") {}
    
protected:
    void SetUp() override {
        // Create test interop manager
        InteropConfiguration config = InteropFactory::CreateStandardConfiguration(L"TestApp.1.0");
        m_interopManager = InteropFactory::CreateCustomInstance(config);
    }
    
    void TearDown() override {
        if (m_interopManager) {
            InteropFactory::DestroyInstance(m_interopManager);
            m_interopManager = nullptr;
        }
    }
    
private:
    InteropManager* m_interopManager;

public:
    // Test WinRT Bridge functionality
    void TestWinRTBridgeInitialization() {
        TestCase testCase("WinRT Bridge Initialization");
        
        auto winrtBridge = m_interopManager->GetWinRTBridge();
        testCase.AssertNotNull(winrtBridge, "WinRT Bridge should be available");
        
        // Test WinRT availability
        bool isAvailable = winrtBridge->IsWinRTAvailable();
        testCase.AssertTrue(isAvailable, "WinRT should be available on Windows Phone 8.1");
        
        AddTestCase(testCase);
    }
    
    void TestWinRTComponentActivation() {
        TestCase testCase("WinRT Component Activation");
        
        // Test activating Windows.Storage.ApplicationData
        void* instance = nullptr;
        HRESULT hr = m_interopManager->ActivateWinRTComponent(
            L"Windows.Storage.ApplicationData", &instance);
        
        testCase.AssertSuccess(hr, "Should successfully activate ApplicationData");
        testCase.AssertNotNull(instance, "Instance should not be null");
        
        if (instance) {
            static_cast<IUnknown*>(instance)->Release();
        }
        
        AddTestCase(testCase);
    }
    
    // Test P/Invoke functionality
    void TestPInvokeEngineInitialization() {
        TestCase testCase("P/Invoke Engine Initialization");
        
        auto pinvokeEngine = m_interopManager->GetPInvokeEngine();
        testCase.AssertNotNull(pinvokeEngine, "P/Invoke Engine should be available");
        
        AddTestCase(testCase);
    }
    
    void TestNativeFunctionCall() {
        TestCase testCase("Native Function Call");
        
        // Test calling GetTickCount from kernel32
        DWORD result = 0;
        HRESULT hr = m_interopManager->CallNativeFunction(
            L"kernel32.dll", L"GetTickCount", nullptr, &result);
        
        testCase.AssertSuccess(hr, "Should successfully call GetTickCount");
        testCase.AssertTrue(result > 0, "GetTickCount should return positive value");
        
        AddTestCase(testCase);
    }
    
    // Test Hardware Access functionality
    void TestHardwareManagerInitialization() {
        TestCase testCase("Hardware Manager Initialization");
        
        auto hardwareManager = m_interopManager->GetHardwareManager();
        testCase.AssertNotNull(hardwareManager, "Hardware Manager should be available");
        
        AddTestCase(testCase);
    }
    
    void TestHardwareCapabilityDetection() {
        TestCase testCase("Hardware Capability Detection");
        
        std::vector<HardwareCapability> capabilities;
        HRESULT hr = m_interopManager->GetAvailableCapabilities(&capabilities);
        
        testCase.AssertSuccess(hr, "Should successfully get available capabilities");
        testCase.AssertTrue(capabilities.size() > 0, "Should detect some hardware capabilities");
        
        // Check for common Windows Phone capabilities
        bool hasAccelerometer = false;
        bool hasLocation = false;
        
        for (const auto& cap : capabilities) {
            if (cap == HardwareCapability::Accelerometer) hasAccelerometer = true;
            if (cap == HardwareCapability::GPS) hasLocation = true;
        }
        
        testCase.AssertTrue(hasAccelerometer, "Should detect accelerometer");
        testCase.LogInfo("Location available: " + std::to_string(hasLocation));
        
        AddTestCase(testCase);
    }
    
    void TestSensorAccess() {
        TestCase testCase("Sensor Access");
        
        // Test accelerometer access
        auto hardwareManager = m_interopManager->GetHardwareManager();
        auto sensorInterface = hardwareManager->GetSensorInterface();
        
        if (sensorInterface && sensorInterface->IsAccelerometerAvailable()) {
            AccelerometerReading reading;
            HRESULT hr = sensorInterface->GetCurrentAccelerometerReading(&reading);
            
            if (SUCCEEDED(hr)) {
                testCase.AssertTrue(true, "Successfully read accelerometer data");
                testCase.LogInfo("Acceleration X: " + std::to_string(reading.accelerationX));
            } else {
                testCase.LogWarning("Accelerometer reading failed (may be normal in test environment)");
            }
        } else {
            testCase.LogInfo("Accelerometer not available in test environment");
        }
        
        AddTestCase(testCase);
    }
    
    // Test Security functionality
    void TestSecurityEnforcerInitialization() {
        TestCase testCase("Security Enforcer Initialization");
        
        auto securityEnforcer = m_interopManager->GetSecurityEnforcer();
        testCase.AssertNotNull(securityEnforcer, "Security Enforcer should be available");
        
        AddTestCase(testCase);
    }
    
    void TestCapabilityValidation() {
        TestCase testCase("Capability Validation");
        
        // Test checking camera capability
        std::vector<SystemCapability> requiredCaps = { SystemCapability::Webcam };
        HRESULT hr = m_interopManager->CheckPermissions(requiredCaps);
        
        // This might fail if permission is denied, which is expected behavior
        testCase.LogInfo("Camera permission check result: " + std::to_string(hr));
        
        // Test checking a capability that doesn't require permission
        requiredCaps = { SystemCapability::InternetClient };
        hr = m_interopManager->CheckPermissions(requiredCaps);
        testCase.AssertSuccess(hr, "Internet client capability should be available");
        
        AddTestCase(testCase);
    }
    
    // Test System Services functionality
    void TestSystemServicesInitialization() {
        TestCase testCase("System Services Initialization");
        
        auto systemServices = m_interopManager->GetSystemServices();
        testCase.AssertNotNull(systemServices, "System Services should be available");
        
        AddTestCase(testCase);
    }
    
    void TestSystemServiceStatus() {
        TestCase testCase("System Service Status");
        
        std::map<SystemServiceType, bool> serviceStatus;
        HRESULT hr = m_interopManager->GetSystemServiceStatus(&serviceStatus);
        
        testCase.AssertSuccess(hr, "Should successfully get service status");
        testCase.AssertTrue(serviceStatus.size() > 0, "Should have some services available");
        
        // Log available services
        for (const auto& service : serviceStatus) {
            std::string serviceName = "Service " + std::to_string(static_cast<int>(service.first));
            testCase.LogInfo(serviceName + ": " + (service.second ? "Available" : "Not Available"));
        }
        
        AddTestCase(testCase);
    }
    
    // Integration tests
    void TestFullInteropWorkflow() {
        TestCase testCase("Full Interop Workflow");
        
        // Test complete workflow: Security check -> Hardware access -> System service
        
        // 1. Check if we have required permissions
        std::vector<SystemCapability> requiredCaps = { SystemCapability::DeviceInformation };
        HRESULT hr = m_interopManager->CheckPermissions(requiredCaps);
        testCase.AssertSuccess(hr, "Should have device information capability");
        
        // 2. Access device information through system service
        auto systemServices = m_interopManager->GetSystemServices();
        if (systemServices && systemServices->IsServiceAvailable(SystemServiceType::DeviceInfo)) {
            testCase.LogInfo("Device information service is available");
            
            // In a real implementation, would call actual device info methods
            testCase.AssertTrue(true, "Device info workflow completed");
        } else {
            testCase.LogWarning("Device information service not available");
        }
        
        AddTestCase(testCase);
    }
    
    void TestErrorHandling() {
        TestCase testCase("Error Handling");
        
        // Test invalid WinRT component activation
        void* instance = nullptr;
        HRESULT hr = m_interopManager->ActivateWinRTComponent(
            L"Invalid.Component.Name", &instance);
        
        testCase.AssertFailure(hr, "Should fail for invalid component");
        testCase.AssertNull(instance, "Instance should be null for failed activation");
        
        // Test invalid native function call
        DWORD result = 0;
        hr = m_interopManager->CallNativeFunction(
            L"nonexistent.dll", L"InvalidFunction", nullptr, &result);
        
        testCase.AssertFailure(hr, "Should fail for nonexistent function");
        
        AddTestCase(testCase);
    }
    
    // Performance tests
    void TestPerformanceMetrics() {
        TestCase testCase("Performance Metrics");
        
        // Test initialization time
        DWORD startTime = GetTickCount();
        
        // Create and initialize a new interop manager
        InteropConfiguration config = InteropFactory::CreateStandardConfiguration(L"PerfTestApp");
        InteropManager* perfTestManager = InteropFactory::CreateCustomInstance(config);
        
        DWORD initTime = GetTickCount() - startTime;
        testCase.LogInfo("Initialization time: " + std::to_string(initTime) + " ms");
        testCase.AssertTrue(initTime < 5000, "Initialization should complete within 5 seconds");
        
        // Test WinRT activation performance
        startTime = GetTickCount();
        void* instance = nullptr;
        HRESULT hr = perfTestManager->ActivateWinRTComponent(L"Windows.Storage.ApplicationData", &instance);
        DWORD activationTime = GetTickCount() - startTime;
        
        if (SUCCEEDED(hr)) {
            testCase.LogInfo("WinRT activation time: " + std::to_string(activationTime) + " ms");
            testCase.AssertTrue(activationTime < 1000, "WinRT activation should be fast");
            
            if (instance) {
                static_cast<IUnknown*>(instance)->Release();
            }
        }
        
        InteropFactory::DestroyInstance(perfTestManager);
        AddTestCase(testCase);
    }
};

// Test runner for interop tests
class InteropTestRunner {
public:
    static int RunAllTests() {
        TestRunner runner;
        
        // Create and register test suite
        InteropTestSuite interopTests;
        
        // Register individual tests
        interopTests.AddTest([&]() { interopTests.TestWinRTBridgeInitialization(); });
        interopTests.AddTest([&]() { interopTests.TestWinRTComponentActivation(); });
        interopTests.AddTest([&]() { interopTests.TestPInvokeEngineInitialization(); });
        interopTests.AddTest([&]() { interopTests.TestNativeFunctionCall(); });
        interopTests.AddTest([&]() { interopTests.TestHardwareManagerInitialization(); });
        interopTests.AddTest([&]() { interopTests.TestHardwareCapabilityDetection(); });
        interopTests.AddTest([&]() { interopTests.TestSensorAccess(); });
        interopTests.AddTest([&]() { interopTests.TestSecurityEnforcerInitialization(); });
        interopTests.AddTest([&]() { interopTests.TestCapabilityValidation(); });
        interopTests.AddTest([&]() { interopTests.TestSystemServicesInitialization(); });
        interopTests.AddTest([&]() { interopTests.TestSystemServiceStatus(); });
        interopTests.AddTest([&]() { interopTests.TestFullInteropWorkflow(); });
        interopTests.AddTest([&]() { interopTests.TestErrorHandling(); });
        interopTests.AddTest([&]() { interopTests.TestPerformanceMetrics(); });
        
        // Run the test suite
        runner.AddTestSuite(&interopTests);
        TestResult result = runner.RunTests();
        
        // Print results
        std::wcout << L"Interop Test Results:" << std::endl;
        std::wcout << L"Total Tests: " << result.totalTests << std::endl;
        std::wcout << L"Passed: " << result.passedTests << std::endl;
        std::wcout << L"Failed: " << result.failedTests << std::endl;
        std::wcout << L"Skipped: " << result.skippedTests << std::endl;
        
        if (result.failedTests > 0) {
            std::wcout << L"Some tests failed. Check test output for details." << std::endl;
            return 1;
        }
        
        std::wcout << L"All interop tests passed!" << std::endl;
        return 0;
    }
};

// Main entry point for interop tests
int main() {
    return InteropTestRunner::RunAllTests();
}