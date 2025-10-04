#include "HardwareAccess.h"
#include <windows.devices.enumeration.h>
#include <ppltasks.h>

using namespace concurrency;
using namespace Windows::Devices::Enumeration;

namespace CLRNet {
namespace Interop {

// DeviceCapabilityDetector Implementation
DeviceCapabilityDetector::DeviceCapabilityDetector() : m_initialized(false) {
}

DeviceCapabilityDetector::~DeviceCapabilityDetector() {
}

HRESULT DeviceCapabilityDetector::Initialize() {
    if (m_initialized) {
        return S_OK;
    }
    
    HRESULT hr = DetectCapabilities();
    if (SUCCEEDED(hr)) {
        m_initialized = true;
    }
    
    return hr;
}

HRESULT DeviceCapabilityDetector::DetectCapabilities() {
    HRESULT hr = S_OK;
    
    // Detect various hardware capabilities
    hr = DetectSensorCapabilities();
    if (FAILED(hr)) return hr;
    
    hr = DetectLocationCapabilities();
    if (FAILED(hr)) return hr;
    
    hr = DetectMediaCapabilities();
    if (FAILED(hr)) return hr;
    
    hr = DetectSystemCapabilities();
    if (FAILED(hr)) return hr;
    
    return S_OK;
}

HRESULT DeviceCapabilityDetector::DetectSensorCapabilities() {
    // Check for accelerometer
    auto accelerometer = Accelerometer::GetDefault();
    AddCapability(HardwareCapability::Accelerometer, 
                 accelerometer != nullptr, L"", L"Default Accelerometer");
    
    // Check for gyrometer
    auto gyrometer = Gyrometer::GetDefault();
    AddCapability(HardwareCapability::Gyroscope, 
                 gyrometer != nullptr, L"", L"Default Gyrometer");
    
    // Check for compass
    auto compass = Compass::GetDefault();
    AddCapability(HardwareCapability::Compass, 
                 compass != nullptr, L"", L"Default Compass");
    
    // Check for light sensor
    auto lightSensor = LightSensor::GetDefault();
    AddCapability(HardwareCapability::LightSensor, 
                 lightSensor != nullptr, L"", L"Default Light Sensor");
    
    // Check for proximity sensor
    auto proximitySensor = ProximitySensor::GetDefault();
    AddCapability(HardwareCapability::ProximitySensor, 
                 proximitySensor != nullptr, L"", L"Default Proximity Sensor");
    
    return S_OK;
}

HRESULT DeviceCapabilityDetector::DetectLocationCapabilities() {
    // Check for GPS capability
    try {
        auto geolocator = ref new Geolocator();
        AddCapability(HardwareCapability::GPS, true, L"", L"GPS Location Service");
        AddCapability(HardwareCapability::LocationService, true, L"", L"Location Service");
    }
    catch (...) {
        AddCapability(HardwareCapability::GPS, false);
        AddCapability(HardwareCapability::LocationService, false);
    }
    
    return S_OK;
}

HRESULT DeviceCapabilityDetector::DetectMediaCapabilities() {
    // Detect camera availability
    try {
        // Check for camera devices
        auto deviceSelector = DeviceInformation::GetDeviceSelector(DeviceClass::VideoCapture);
        
        create_task(DeviceInformation::FindAllAsync(deviceSelector))
            .then([this](DeviceInformationCollection^ devices) {
            bool hasCamera = devices->Size > 0;
            AddCapability(HardwareCapability::Camera, hasCamera, 
                         hasCamera ? devices->GetAt(0)->Id->Data() : L"", 
                         hasCamera ? devices->GetAt(0)->Name->Data() : L"");
        });
        
        // Check for microphone
        auto micSelector = DeviceInformation::GetDeviceSelector(DeviceClass::AudioCapture);
        create_task(DeviceInformation::FindAllAsync(micSelector))
            .then([this](DeviceInformationCollection^ devices) {
            bool hasMicrophone = devices->Size > 0;
            AddCapability(HardwareCapability::Microphone, hasMicrophone, 
                         hasMicrophone ? devices->GetAt(0)->Id->Data() : L"", 
                         hasMicrophone ? devices->GetAt(0)->Name->Data() : L"");
        });
    }
    catch (...) {
        AddCapability(HardwareCapability::Camera, false);
        AddCapability(HardwareCapability::Microphone, false);
    }
    
    return S_OK;
}

HRESULT DeviceCapabilityDetector::DetectSystemCapabilities() {
    // Vibration is typically available on Windows Phone
    AddCapability(HardwareCapability::Vibration, true, L"", L"Vibration Motor");
    
    // Device information is always available
    AddCapability(HardwareCapability::DeviceInformation, true, L"", L"Device Information");
    
    // Battery information
    AddCapability(HardwareCapability::Battery, true, L"", L"Battery Status");
    
    return S_OK;
}

void DeviceCapabilityDetector::AddCapability(HardwareCapability capability, bool available, 
                                           const std::wstring& deviceId, 
                                           const std::wstring& deviceName) {
    DeviceCapabilityInfo info;
    info.capability = capability;
    info.isAvailable = available;
    info.isEnabled = available;
    info.deviceId = deviceId;
    info.deviceName = deviceName;
    info.permission = PermissionState::Unknown;
    info.manufacturer = L"";
    info.version = L"";
    
    m_capabilities[capability] = info;
}

bool DeviceCapabilityDetector::IsCapabilityAvailable(HardwareCapability capability) const {
    auto it = m_capabilities.find(capability);
    return (it != m_capabilities.end()) ? it->second.isAvailable : false;
}

// PermissionManager Implementation
PermissionManager::PermissionManager() {
    InitializeCriticalSection(&m_criticalSection);
}

PermissionManager::~PermissionManager() {
    DeleteCriticalSection(&m_criticalSection);
}

HRESULT PermissionManager::Initialize() {
    // Initialize permission states for various capabilities
    m_permissions[HardwareCapability::Camera] = PermissionState::Unknown;
    m_permissions[HardwareCapability::Microphone] = PermissionState::Unknown;
    m_permissions[HardwareCapability::GPS] = PermissionState::Unknown;
    m_permissions[HardwareCapability::LocationService] = PermissionState::Unknown;
    
    // Sensors typically don't require explicit permission on WP8.1
    m_permissions[HardwareCapability::Accelerometer] = PermissionState::Granted;
    m_permissions[HardwareCapability::Gyroscope] = PermissionState::Granted;
    m_permissions[HardwareCapability::Compass] = PermissionState::Granted;
    m_permissions[HardwareCapability::LightSensor] = PermissionState::Granted;
    m_permissions[HardwareCapability::ProximitySensor] = PermissionState::Granted;
    m_permissions[HardwareCapability::Vibration] = PermissionState::Granted;
    
    return S_OK;
}

HRESULT PermissionManager::RequestPermission(HardwareCapability capability, 
                                            PermissionCallback callback) {
    if (!IsPermissionRequired(capability)) {
        // No permission required, grant immediately
        GrantPermission(capability);
        if (callback) {
            callback(PermissionState::Granted);
        }
        return S_OK;
    }
    
    EnterCriticalSection(&m_criticalSection);
    
    // Check current permission state
    auto it = m_permissions.find(capability);
    if (it != m_permissions.end() && it->second == PermissionState::Granted) {
        // Already granted
        if (callback) {
            callback(PermissionState::Granted);
        }
        LeaveCriticalSection(&m_criticalSection);
        return S_OK;
    }
    
    // Store callback for later notification
    if (callback) {
        m_permissionCallbacks[capability] = callback;
    }
    
    // Update state to pending
    UpdatePermissionState(capability, PermissionState::Pending);
    
    LeaveCriticalSection(&m_criticalSection);
    
    // Show permission dialog (simplified implementation)
    return ShowPermissionDialog(capability);
}

HRESULT PermissionManager::ShowPermissionDialog(HardwareCapability capability) {
    // In a real implementation, this would show a system permission dialog
    // For now, we'll simulate user consent
    
    std::wstring capabilityName = GetCapabilityDisplayName(capability);
    std::wstring message = L"Allow access to " + capabilityName + L"?";
    
    // Simulate user granting permission (in real implementation, would be async dialog)
    int result = MessageBoxW(nullptr, message.c_str(), L"Permission Request", MB_YESNO);
    
    PermissionState newState = (result == IDYES) ? PermissionState::Granted : PermissionState::Denied;
    UpdatePermissionState(capability, newState);
    
    // Notify callback
    auto callbackIt = m_permissionCallbacks.find(capability);
    if (callbackIt != m_permissionCallbacks.end()) {
        callbackIt->second(newState);
        m_permissionCallbacks.erase(callbackIt);
    }
    
    return S_OK;
}

void PermissionManager::UpdatePermissionState(HardwareCapability capability, PermissionState state) {
    EnterCriticalSection(&m_criticalSection);
    m_permissions[capability] = state;
    LeaveCriticalSection(&m_criticalSection);
}

std::wstring PermissionManager::GetCapabilityDisplayName(HardwareCapability capability) const {
    switch (capability) {
        case HardwareCapability::Camera: return L"Camera";
        case HardwareCapability::Microphone: return L"Microphone";
        case HardwareCapability::GPS: return L"Location (GPS)";
        case HardwareCapability::LocationService: return L"Location Services";
        case HardwareCapability::Accelerometer: return L"Motion Sensors";
        case HardwareCapability::Gyroscope: return L"Motion Sensors";
        case HardwareCapability::Compass: return L"Compass";
        default: return L"Unknown";
    }
}

bool PermissionManager::IsPermissionRequired(HardwareCapability capability) const {
    // On Windows Phone 8.1, certain capabilities require explicit permission
    switch (capability) {
        case HardwareCapability::Camera:
        case HardwareCapability::Microphone:
        case HardwareCapability::GPS:
        case HardwareCapability::LocationService:
            return true;
        default:
            return false;
    }
}

// CameraInterface Implementation
CameraInterface::CameraInterface() : m_initialized(false), m_isCapturing(false) {
}

CameraInterface::~CameraInterface() {
    Shutdown();
}

HRESULT CameraInterface::Initialize(const std::wstring& deviceId) {
    if (m_initialized) {
        return S_OK;
    }
    
    m_deviceId = deviceId;
    
    try {
        HRESULT hr = InitializeMediaCapture();
        if (SUCCEEDED(hr)) {
            m_initialized = true;
        }
        return hr;
    }
    catch (...) {
        return E_FAIL;
    }
}

HRESULT CameraInterface::InitializeMediaCapture() {
    // Create MediaCapture instance
    m_mediaCapture = ref new MediaCapture();
    
    // Initialize with default settings
    auto settings = ref new MediaCaptureInitializationSettings();
    settings->StreamingCaptureMode = StreamingCaptureMode::VideoAndAudio;
    
    // If specific device ID provided, use it
    if (!m_deviceId.empty()) {
        settings->VideoDeviceId = ref new Platform::String(m_deviceId.c_str());
    }
    
    // Initialize asynchronously (simplified for demo)
    try {
        create_task(m_mediaCapture->InitializeAsync(settings))
            .then([this](task<void> initTask) {
            try {
                initTask.get();
                return S_OK;
            }
            catch (...) {
                return E_FAIL;
            }
        });
        return S_OK;
    }
    catch (...) {
        return E_FAIL;
    }
}

HRESULT CameraInterface::CapturePhoto(const std::wstring& filePath) {
    if (!m_initialized || !m_mediaCapture) {
        return E_NOT_VALID_STATE;
    }
    
    try {
        // Create storage file for photo
        auto folder = Windows::Storage::ApplicationData::Current->LocalFolder;
        auto fileName = ref new Platform::String(filePath.c_str());
        
        create_task(folder->CreateFileAsync(fileName, 
                   Windows::Storage::CreationCollisionOption::ReplaceExisting))
            .then([this](Windows::Storage::StorageFile^ file) {
            // Capture photo to file
            auto imageProperties = ImageEncodingProperties::CreateJpeg();
            return m_mediaCapture->CapturePhotoToStorageFileAsync(imageProperties, file);
        }).then([](task<void> captureTask) {
            try {
                captureTask.get();
                return S_OK;
            }
            catch (...) {
                return E_FAIL;
            }
        });
        
        return S_OK;
    }
    catch (...) {
        return E_FAIL;
    }
}

// SensorInterface Implementation
SensorInterface::SensorInterface() : m_initialized(false) {
}

SensorInterface::~SensorInterface() {
    Shutdown();
}

HRESULT SensorInterface::Initialize() {
    if (m_initialized) {
        return S_OK;
    }
    
    HRESULT hr = S_OK;
    
    // Initialize available sensors
    hr = InitializeAccelerometer();
    if (FAILED(hr)) return hr;
    
    hr = InitializeGyrometer();
    if (FAILED(hr)) return hr;
    
    hr = InitializeCompass();
    if (FAILED(hr)) return hr;
    
    m_initialized = true;
    return S_OK;
}

HRESULT SensorInterface::InitializeAccelerometer() {
    m_accelerometer = Accelerometer::GetDefault();
    return (m_accelerometer != nullptr) ? S_OK : S_FALSE;
}

HRESULT SensorInterface::StartAccelerometerReading(AccelerometerCallback callback) {
    if (!m_accelerometer) {
        return E_NOT_SUPPORTED;
    }
    
    m_accelerometerCallback = callback;
    
    // Set report interval (e.g., 100ms)
    m_accelerometer->ReportInterval = 100;
    
    // Register for reading changed events
    m_accelerometerToken = m_accelerometer->ReadingChanged += 
        ref new Windows::Foundation::TypedEventHandler<Accelerometer^, AccelerometerReadingChangedEventArgs^>(
            [this](Accelerometer^ sender, AccelerometerReadingChangedEventArgs^ args) {
                OnAccelerometerReading(sender, args);
            });
    
    return S_OK;
}

void SensorInterface::OnAccelerometerReading(Accelerometer^ sender, AccelerometerReadingChangedEventArgs^ args) {
    if (m_accelerometerCallback) {
        AccelerometerReading reading;
        reading.accelerationX = args->Reading->AccelerationX;
        reading.accelerationY = args->Reading->AccelerationY;
        reading.accelerationZ = args->Reading->AccelerationZ;
        
        // Convert DateTime to FILETIME
        auto dateTime = args->Reading->Timestamp;
        ULARGE_INTEGER uli;
        uli.QuadPart = dateTime.UniversalTime;
        reading.timestamp.dwLowDateTime = uli.LowPart;
        reading.timestamp.dwHighDateTime = uli.HighPart;
        
        m_accelerometerCallback(reading);
    }
}

// HardwareAccessManager Implementation
HardwareAccessManager::HardwareAccessManager() : m_initialized(false) {
    InitializeCriticalSection(&m_criticalSection);
    m_capabilityDetector = std::make_unique<DeviceCapabilityDetector>();
    m_permissionManager = std::make_unique<PermissionManager>();
    m_cameraInterface = std::make_unique<CameraInterface>();
    m_locationInterface = std::make_unique<LocationInterface>();
    m_sensorInterface = std::make_unique<SensorInterface>();
}

HardwareAccessManager::~HardwareAccessManager() {
    Shutdown();
    DeleteCriticalSection(&m_criticalSection);
}

HRESULT HardwareAccessManager::Initialize() {
    if (m_initialized) {
        return S_OK;
    }
    
    HRESULT hr = S_OK;
    
    // Initialize all subsystems
    hr = m_capabilityDetector->Initialize();
    if (FAILED(hr)) return hr;
    
    hr = m_permissionManager->Initialize();
    if (FAILED(hr)) return hr;
    
    hr = m_sensorInterface->Initialize();
    if (FAILED(hr)) return hr;
    
    m_initialized = true;
    return S_OK;
}

void HardwareAccessManager::Shutdown() {
    if (m_initialized) {
        CleanupResources();
        m_sensorInterface.reset();
        m_locationInterface.reset();
        m_cameraInterface.reset();
        m_permissionManager.reset();
        m_capabilityDetector.reset();
        m_initialized = false;
    }
}

bool HardwareAccessManager::IsCapabilityAvailable(HardwareCapability capability) {
    return m_capabilityDetector->IsCapabilityAvailable(capability);
}

HRESULT HardwareAccessManager::TakePhoto(const std::wstring& filePath) {
    // Check capability and permission
    if (!IsCapabilityAvailable(HardwareCapability::Camera)) {
        return E_NOT_SUPPORTED;
    }
    
    if (m_permissionManager->GetPermissionState(HardwareCapability::Camera) != PermissionState::Granted) {
        return E_ACCESSDENIED;
    }
    
    // Initialize camera if needed
    if (!m_cameraInterface) {
        return E_NOT_VALID_STATE;
    }
    
    HRESULT hr = m_cameraInterface->Initialize();
    if (FAILED(hr)) {
        return hr;
    }
    
    return m_cameraInterface->CapturePhoto(filePath);
}

void HardwareAccessManager::CleanupResources() {
    // Cleanup any active operations
    if (m_cameraInterface) {
        m_cameraInterface->Shutdown();
    }
    if (m_locationInterface) {
        m_locationInterface->Shutdown();
    }
    if (m_sensorInterface) {
        m_sensorInterface->Shutdown();
    }
}

// Factory Implementation
HardwareAccessManager* HardwareAccessFactory::CreateInstance() {
    return new HardwareAccessManager();
}

void HardwareAccessFactory::DestroyInstance(HardwareAccessManager* instance) {
    delete instance;
}

} // namespace Interop
} // namespace CLRNet