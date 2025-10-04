#pragma once

#include <windows.h>
#include <windows.phone.h>
#include <windows.devices.sensors.h>
#include <windows.devices.geolocation.h>
#include <windows.media.capture.h>
#include <wrl/client.h>
#include <map>
#include <string>
#include <vector>
#include <functional>

using namespace Microsoft::WRL;
using namespace Windows::Devices::Sensors;
using namespace Windows::Devices::Geolocation;
using namespace Windows::Media::Capture;

namespace CLRNet {
namespace Interop {

// Hardware capabilities enumeration
enum class HardwareCapability {
    // Sensors
    Accelerometer,
    Gyroscope,
    Magnetometer,
    Compass,
    LightSensor,
    ProximitySensor,
    Inclinometer,
    Barometer,
    
    // Location services
    GPS,
    LocationService,
    
    // Media devices
    Camera,
    Microphone,
    
    // Communication
    Vibration,
    
    // System
    DeviceInformation,
    Battery
};

// Permission states for hardware access
enum class PermissionState {
    Unknown,
    Denied,
    Granted,
    Pending,
    Restricted
};

// Device capability information
struct DeviceCapabilityInfo {
    HardwareCapability capability;
    bool isAvailable;
    bool isEnabled;
    std::wstring deviceId;
    std::wstring deviceName;
    PermissionState permission;
    std::wstring manufacturer;
    std::wstring version;
};

// Sensor reading data structures
struct AccelerometerReading {
    double accelerationX;
    double accelerationY;
    double accelerationZ;
    FILETIME timestamp;
};

struct GyroscopeReading {
    double angularVelocityX;
    double angularVelocityY;
    double angularVelocityZ;
    FILETIME timestamp;
};

struct CompassReading {
    double headingMagneticNorth;
    double headingTrueNorth;
    FILETIME timestamp;
};

struct LocationReading {
    double latitude;
    double longitude;
    double altitude;
    double accuracy;
    double speed;
    double heading;
    FILETIME timestamp;
};

// Event callback types
typedef std::function<void(const AccelerometerReading&)> AccelerometerCallback;
typedef std::function<void(const GyroscopeReading&)> GyroscopeCallback;
typedef std::function<void(const CompassReading&)> CompassCallback;
typedef std::function<void(const LocationReading&)> LocationCallback;
typedef std::function<void(PermissionState)> PermissionCallback;

// Forward declarations
class DeviceCapabilityDetector;
class PermissionManager;
class CameraInterface;
class LocationInterface;
class SensorInterface;

// Device capability detector
class DeviceCapabilityDetector {
private:
    std::map<HardwareCapability, DeviceCapabilityInfo> m_capabilities;
    bool m_initialized;

public:
    DeviceCapabilityDetector();
    ~DeviceCapabilityDetector();
    
    // Initialize capability detection
    HRESULT Initialize();
    
    // Detect all available capabilities
    HRESULT DetectCapabilities();
    
    // Check if specific capability is available
    bool IsCapabilityAvailable(HardwareCapability capability) const;
    
    // Get capability information
    HRESULT GetCapabilityInfo(HardwareCapability capability, 
                             DeviceCapabilityInfo* info) const;
    
    // Get all available capabilities
    std::vector<HardwareCapability> GetAvailableCapabilities() const;
    
    // Refresh capability status
    HRESULT RefreshCapabilities();

private:
    // Detection methods for different hardware
    HRESULT DetectSensorCapabilities();
    HRESULT DetectLocationCapabilities();
    HRESULT DetectMediaCapabilities();
    HRESULT DetectSystemCapabilities();
    
    // Helper methods
    void AddCapability(HardwareCapability capability, bool available, 
                      const std::wstring& deviceId = L"", 
                      const std::wstring& deviceName = L"");
};

// Permission management system
class PermissionManager {
private:
    std::map<HardwareCapability, PermissionState> m_permissions;
    std::map<HardwareCapability, PermissionCallback> m_permissionCallbacks;
    CRITICAL_SECTION m_criticalSection;

public:
    PermissionManager();
    ~PermissionManager();
    
    // Initialize permission system
    HRESULT Initialize();
    
    // Check current permission state
    PermissionState GetPermissionState(HardwareCapability capability) const;
    
    // Request permission for capability
    HRESULT RequestPermission(HardwareCapability capability, 
                             PermissionCallback callback);
    
    // Grant permission (for system use)
    HRESULT GrantPermission(HardwareCapability capability);
    
    // Deny permission (for system use)
    HRESULT DenyPermission(HardwareCapability capability);
    
    // Check if permission is required for capability
    bool IsPermissionRequired(HardwareCapability capability) const;
    
    // Set permission callback
    void SetPermissionCallback(HardwareCapability capability, 
                              PermissionCallback callback);

private:
    // Internal permission handling
    HRESULT ShowPermissionDialog(HardwareCapability capability);
    void UpdatePermissionState(HardwareCapability capability, PermissionState state);
    std::wstring GetCapabilityDisplayName(HardwareCapability capability) const;
};

// Camera interface
class CameraInterface {
private:
    ComPtr<MediaCapture> m_mediaCapture;
    bool m_initialized;
    bool m_isCapturing;
    std::wstring m_deviceId;

public:
    CameraInterface();
    ~CameraInterface();
    
    // Initialize camera
    HRESULT Initialize(const std::wstring& deviceId = L"");
    
    // Shutdown camera
    void Shutdown();
    
    // Start video preview
    HRESULT StartPreview();
    
    // Stop video preview
    HRESULT StopPreview();
    
    // Capture photo
    HRESULT CapturePhoto(const std::wstring& filePath);
    
    // Start video recording
    HRESULT StartVideoRecording(const std::wstring& filePath);
    
    // Stop video recording
    HRESULT StopVideoRecording();
    
    // Get camera capabilities
    HRESULT GetCameraCapabilities();
    
    // Check if camera is available
    bool IsCameraAvailable() const;
    
    // Get available camera devices
    static std::vector<std::wstring> GetAvailableCameras();

private:
    HRESULT InitializeMediaCapture();
    HRESULT ConfigureCameraSettings();
};

// Location service interface
class LocationInterface {
private:
    ComPtr<Geolocator> m_geolocator;
    bool m_initialized;
    bool m_isTracking;
    LocationCallback m_locationCallback;
    Windows::Foundation::EventRegistrationToken m_positionChangedToken;

public:
    LocationInterface();
    ~LocationInterface();
    
    // Initialize location service
    HRESULT Initialize();
    
    // Shutdown location service
    void Shutdown();
    
    // Get current location (single reading)
    HRESULT GetCurrentLocation(LocationReading* location);
    
    // Start continuous location tracking
    HRESULT StartLocationTracking(LocationCallback callback);
    
    // Stop location tracking
    HRESULT StopLocationTracking();
    
    // Set desired accuracy
    HRESULT SetDesiredAccuracy(PositionAccuracy accuracy);
    
    // Set movement threshold
    HRESULT SetMovementThreshold(double meters);
    
    // Check if location is available
    bool IsLocationAvailable() const;

private:
    HRESULT ConfigureGeolocator();
    void OnPositionChanged(Geolocator^ sender, PositionChangedEventArgs^ args);
};

// Sensor interface collection
class SensorInterface {
private:
    // Sensor instances
    ComPtr<Accelerometer> m_accelerometer;
    ComPtr<Gyrometer> m_gyrometer;
    ComPtr<Compass> m_compass;
    ComPtr<LightSensor> m_lightSensor;
    ComPtr<ProximitySensor> m_proximitySensor;
    ComPtr<Inclinometer> m_inclinometer;
    ComPtr<Barometer> m_barometer;
    
    // Event callbacks
    AccelerometerCallback m_accelerometerCallback;
    GyroscopeCallback m_gyroscopeCallback;
    CompassCallback m_compassCallback;
    
    // Event tokens for cleanup
    Windows::Foundation::EventRegistrationToken m_accelerometerToken;
    Windows::Foundation::EventRegistrationToken m_gyrometerToken;
    Windows::Foundation::EventRegistrationToken m_compassToken;
    
    bool m_initialized;

public:
    SensorInterface();
    ~SensorInterface();
    
    // Initialize sensor system
    HRESULT Initialize();
    
    // Shutdown sensor system
    void Shutdown();
    
    // Accelerometer methods
    HRESULT StartAccelerometerReading(AccelerometerCallback callback);
    HRESULT StopAccelerometerReading();
    HRESULT GetCurrentAccelerometerReading(AccelerometerReading* reading);
    
    // Gyroscope methods
    HRESULT StartGyroscopeReading(GyroscopeCallback callback);
    HRESULT StopGyroscopeReading();
    HRESULT GetCurrentGyroscopeReading(GyroscopeReading* reading);
    
    // Compass methods
    HRESULT StartCompassReading(CompassCallback callback);
    HRESULT StopCompassReading();
    HRESULT GetCurrentCompassReading(CompassReading* reading);
    
    // Check sensor availability
    bool IsAccelerometerAvailable() const;
    bool IsGyroscopeAvailable() const;
    bool IsCompassAvailable() const;
    bool IsLightSensorAvailable() const;
    bool IsProximitySensorAvailable() const;

private:
    // Sensor initialization helpers
    HRESULT InitializeAccelerometer();
    HRESULT InitializeGyrometer();
    HRESULT InitializeCompass();
    
    // Event handlers
    void OnAccelerometerReading(Accelerometer^ sender, AccelerometerReadingChangedEventArgs^ args);
    void OnGyrometerReading(Gyrometer^ sender, GyrometerReadingChangedEventArgs^ args);
    void OnCompassReading(Compass^ sender, CompassReadingChangedEventArgs^ args);
};

// Main hardware access manager
class HardwareAccessManager {
private:
    std::unique_ptr<DeviceCapabilityDetector> m_capabilityDetector;
    std::unique_ptr<PermissionManager> m_permissionManager;
    std::unique_ptr<CameraInterface> m_cameraInterface;
    std::unique_ptr<LocationInterface> m_locationInterface;
    std::unique_ptr<SensorInterface> m_sensorInterface;
    
    bool m_initialized;
    CRITICAL_SECTION m_criticalSection;

public:
    HardwareAccessManager();
    ~HardwareAccessManager();
    
    // Initialize hardware access system
    HRESULT Initialize();
    
    // Shutdown hardware access system
    void Shutdown();
    
    // Capability detection
    bool IsCapabilityAvailable(HardwareCapability capability);
    std::vector<HardwareCapability> GetAvailableCapabilities();
    HRESULT GetCapabilityInfo(HardwareCapability capability, DeviceCapabilityInfo* info);
    
    // Permission management
    HRESULT RequestPermission(HardwareCapability capability, PermissionCallback callback);
    PermissionState GetPermissionState(HardwareCapability capability);
    
    // Hardware interfaces
    CameraInterface* GetCameraInterface();
    LocationInterface* GetLocationInterface();
    SensorInterface* GetSensorInterface();
    
    // Convenience methods
    HRESULT TakePhoto(const std::wstring& filePath);
    HRESULT GetCurrentLocation(LocationReading* location);
    HRESULT StartSensorMonitoring(HardwareCapability sensor);
    HRESULT StopSensorMonitoring(HardwareCapability sensor);
    
    // System integration
    HRESULT Vibrate(DWORD milliseconds);
    HRESULT GetBatteryLevel(int* batteryLevel);
    HRESULT GetDeviceInformation(std::wstring* deviceName, std::wstring* manufacturer);

private:
    // Internal helper methods
    HRESULT ValidatePermissions(HardwareCapability capability);
    void CleanupResources();
};

// Hardware access factory
class HardwareAccessFactory {
public:
    static HardwareAccessManager* CreateInstance();
    static void DestroyInstance(HardwareAccessManager* instance);
    
private:
    HardwareAccessFactory() = default;
};

} // namespace Interop
} // namespace CLRNet