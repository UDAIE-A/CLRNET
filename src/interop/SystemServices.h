#pragma once

#include <windows.h>
#include <windows.phone.h>
#include <windows.applicationmodel.h>
#include <windows.applicationmodel.calls.h>
#include <windows.applicationmodel.contacts.h>
#include <windows.applicationmodel.appointments.h>
#include <windows.applicationmodel.background.h>
#include <windows.ui.notifications.h>
#include <wrl/client.h>
#include <map>
#include <string>
#include <vector>
#include <functional>

using namespace Microsoft::WRL;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Calls;
using namespace Windows::ApplicationModel::Contacts;
using namespace Windows::ApplicationModel::Appointments;
using namespace Windows::ApplicationModel::Background;
using namespace Windows::UI::Notifications;

namespace CLRNet {
namespace Interop {

// System service types
enum class SystemServiceType {
    PhoneDialer,
    SMS,
    Contacts,
    Calendar,
    Email,
    PushNotifications,
    BackgroundTasks,
    AppLifecycle,
    SystemNotifications,
    DeviceInfo,
    Storage
};

// Call status enumeration
enum class CallStatus {
    Idle,
    Dialing,
    Ringing,
    Connected,
    Hold,
    Ended,
    Busy,
    Failed
};

// SMS status enumeration
enum class SMSStatus {
    Sent,
    Failed,
    Pending,
    Delivered,
    Received
};

// Background task trigger types
enum class BackgroundTriggerType {
    TimeTrigger,
    SystemTrigger,
    MaintenanceTrigger,
    PushNotificationTrigger,
    LocationTrigger,
    NetworkStateChange,
    InternetAvailable
};

// App lifecycle states
enum class AppLifecycleState {
    NotRunning,
    Activated,
    Running,
    Suspended,
    Terminated,
    ClosedByUser
};

// Forward declarations
class PhoneDialerService;
class SMSService;
class ContactsService;
class CalendarService;
class BackgroundTaskService;
class PushNotificationService;
class AppLifecycleManager;

// Call information structure
struct CallInfo {
    std::wstring phoneNumber;
    std::wstring contactName;
    CallStatus status;
    FILETIME startTime;
    FILETIME endTime;
    DWORD duration;
    bool isIncoming;
    bool isVideoCall;
};

// SMS message structure
struct SMSMessage {
    std::wstring recipientNumber;
    std::wstring senderNumber;
    std::wstring messageBody;
    SMSStatus status;
    FILETIME timestamp;
    bool isIncoming;
    DWORD messageId;
};

// Contact information structure
struct ContactInfo {
    std::wstring contactId;
    std::wstring displayName;
    std::wstring firstName;
    std::wstring lastName;
    std::vector<std::wstring> phoneNumbers;
    std::vector<std::wstring> emailAddresses;
    std::wstring notes;
    FILETIME lastModified;
};

// Calendar appointment structure
struct AppointmentInfo {
    std::wstring appointmentId;
    std::wstring subject;
    std::wstring location;
    std::wstring description;
    FILETIME startTime;
    FILETIME endTime;
    bool isAllDay;
    bool isRecurring;
    std::vector<std::wstring> attendees;
};

// Background task information
struct BackgroundTaskInfo {
    std::wstring taskId;
    std::wstring taskName;
    BackgroundTriggerType triggerType;
    std::wstring entryPoint;
    bool isEnabled;
    FILETIME registrationTime;
    DWORD executionCount;
    FILETIME lastExecution;
};

// Push notification data
struct PushNotificationData {
    std::wstring channelUri;
    std::wstring notificationContent;
    std::wstring notificationTitle;
    std::wstring launchArgs;
    FILETIME timestamp;
    bool isBadgeUpdate;
    bool isTileUpdate;
    bool isToastNotification;
};

// Event callback types
typedef std::function<void(const CallInfo&)> CallStatusCallback;
typedef std::function<void(const SMSMessage&)> SMSCallback;
typedef std::function<void(const ContactInfo&)> ContactChangedCallback;
typedef std::function<void(const AppointmentInfo&)> CalendarCallback;
typedef std::function<void(const BackgroundTaskInfo&)> BackgroundTaskCallback;
typedef std::function<void(const PushNotificationData&)> PushNotificationCallback;
typedef std::function<void(AppLifecycleState, AppLifecycleState)> LifecycleStateCallback;

// Phone dialer service
class PhoneDialerService {
private:
    ComPtr<PhoneCallManager> m_callManager;
    CallStatusCallback m_callStatusCallback;
    std::map<std::wstring, CallInfo> m_activeCalls;
    bool m_initialized;
    Windows::Foundation::EventRegistrationToken m_callStateToken;

public:
    PhoneDialerService();
    ~PhoneDialerService();
    
    // Initialize phone dialer service
    HRESULT Initialize();
    
    // Shutdown service
    void Shutdown();
    
    // Make a phone call
    HRESULT MakeCall(const std::wstring& phoneNumber, bool isVideoCall = false);
    
    // End active call
    HRESULT EndCall(const std::wstring& callId);
    
    // Hold/resume call
    HRESULT HoldCall(const std::wstring& callId);
    HRESULT ResumeCall(const std::wstring& callId);
    
    // Get active calls
    std::vector<CallInfo> GetActiveCalls() const;
    
    // Check if calling is available
    bool IsCallingAvailable() const;
    
    // Set call status callback
    void SetCallStatusCallback(CallStatusCallback callback);
    
    // Check call capabilities
    bool IsVideoCallingSupported() const;
    bool IsVoIPSupported() const;

private:
    HRESULT InitializeCallManager();
    void OnCallStateChanged(PhoneCall^ call);
    CallInfo CreateCallInfoFromPhoneCall(PhoneCall^ phoneCall);
};

// SMS service
class SMSService {
private:
    ComPtr<SmsDevice> m_smsDevice;
    SMSCallback m_smsCallback;
    std::vector<SMSMessage> m_messageHistory;
    bool m_initialized;
    Windows::Foundation::EventRegistrationToken m_messageReceivedToken;

public:
    SMSService();
    ~SMSService();
    
    // Initialize SMS service
    HRESULT Initialize();
    
    // Shutdown service
    void Shutdown();
    
    // Send SMS message
    HRESULT SendSMS(const std::wstring& phoneNumber, const std::wstring& message);
    
    // Send SMS to multiple recipients
    HRESULT SendBulkSMS(const std::vector<std::wstring>& phoneNumbers, 
                       const std::wstring& message);
    
    // Get message history
    std::vector<SMSMessage> GetMessageHistory(const std::wstring& phoneNumber = L"") const;
    
    // Delete message
    HRESULT DeleteMessage(DWORD messageId);
    
    // Check if SMS is available
    bool IsSMSAvailable() const;
    
    // Set SMS callback for incoming messages
    void SetSMSCallback(SMSCallback callback);
    
    // Get SMS device capabilities
    HRESULT GetSMSCapabilities(DWORD* maxMessageLength, DWORD* maxRecipients);

private:
    HRESULT InitializeSMSDevice();
    void OnSMSMessageReceived(SmsDevice^ sender, SmsMessageReceivedEventArgs^ args);
    SMSMessage CreateSMSMessageFromReceived(SmsTextMessage^ message);
};

// Contacts service
class ContactsService {
private:
    ComPtr<ContactStore> m_contactStore;
    ContactChangedCallback m_contactChangedCallback;
    std::map<std::wstring, ContactInfo> m_contactsCache;
    bool m_initialized;
    Windows::Foundation::EventRegistrationToken m_contactChangedToken;

public:
    ContactsService();
    ~ContactsService();
    
    // Initialize contacts service
    HRESULT Initialize();
    
    // Shutdown service
    void Shutdown();
    
    // Get all contacts
    HRESULT GetAllContacts(std::vector<ContactInfo>* contacts);
    
    // Get contact by ID
    HRESULT GetContact(const std::wstring& contactId, ContactInfo* contact);
    
    // Search contacts
    HRESULT SearchContacts(const std::wstring& searchTerm, std::vector<ContactInfo>* contacts);
    
    // Create new contact
    HRESULT CreateContact(const ContactInfo& contact, std::wstring* contactId);
    
    // Update existing contact
    HRESULT UpdateContact(const ContactInfo& contact);
    
    // Delete contact
    HRESULT DeleteContact(const std::wstring& contactId);
    
    // Check if contacts access is available
    bool IsContactsAvailable() const;
    
    // Set contact changed callback
    void SetContactChangedCallback(ContactChangedCallback callback);

private:
    HRESULT InitializeContactStore();
    ContactInfo CreateContactInfoFromContact(Contact^ contact);
    Contact^ CreateContactFromContactInfo(const ContactInfo& info);
    void OnContactChanged(ContactStore^ sender, ContactChangedEventArgs^ args);
};

// Calendar service
class CalendarService {
private:
    ComPtr<AppointmentStore> m_appointmentStore;
    CalendarCallback m_calendarCallback;
    std::vector<AppointmentInfo> m_appointmentsCache;
    bool m_initialized;

public:
    CalendarService();
    ~CalendarService();
    
    // Initialize calendar service
    HRESULT Initialize();
    
    // Shutdown service
    void Shutdown();
    
    // Get appointments in date range
    HRESULT GetAppointments(const FILETIME& startTime, const FILETIME& endTime,
                           std::vector<AppointmentInfo>* appointments);
    
    // Get appointment by ID
    HRESULT GetAppointment(const std::wstring& appointmentId, AppointmentInfo* appointment);
    
    // Create new appointment
    HRESULT CreateAppointment(const AppointmentInfo& appointment, std::wstring* appointmentId);
    
    // Update existing appointment
    HRESULT UpdateAppointment(const AppointmentInfo& appointment);
    
    // Delete appointment
    HRESULT DeleteAppointment(const std::wstring& appointmentId);
    
    // Search appointments
    HRESULT SearchAppointments(const std::wstring& searchTerm,
                              std::vector<AppointmentInfo>* appointments);
    
    // Check if calendar access is available
    bool IsCalendarAvailable() const;
    
    // Set calendar callback
    void SetCalendarCallback(CalendarCallback callback);

private:
    HRESULT InitializeAppointmentStore();
    AppointmentInfo CreateAppointmentInfoFromAppointment(Appointment^ appointment);
    Appointment^ CreateAppointmentFromAppointmentInfo(const AppointmentInfo& info);
};

// Background task service
class BackgroundTaskService {
private:
    std::map<std::wstring, BackgroundTaskInfo> m_registeredTasks;
    BackgroundTaskCallback m_taskCallback;
    bool m_initialized;
    CRITICAL_SECTION m_criticalSection;

public:
    BackgroundTaskService();
    ~BackgroundTaskService();
    
    // Initialize background task service
    HRESULT Initialize();
    
    // Shutdown service
    void Shutdown();
    
    // Register background task
    HRESULT RegisterBackgroundTask(const std::wstring& taskName,
                                  const std::wstring& entryPoint,
                                  BackgroundTriggerType triggerType,
                                  DWORD triggerInterval = 0);
    
    // Unregister background task
    HRESULT UnregisterBackgroundTask(const std::wstring& taskName);
    
    // Get registered tasks
    std::vector<BackgroundTaskInfo> GetRegisteredTasks() const;
    
    // Check if task is registered
    bool IsTaskRegistered(const std::wstring& taskName) const;
    
    // Request background execution time
    HRESULT RequestBackgroundExecution(const std::wstring& taskName, DWORD maxExecutionTime);
    
    // Check background execution status
    bool IsBackgroundExecutionEnabled() const;
    
    // Set background task callback
    void SetBackgroundTaskCallback(BackgroundTaskCallback callback);

private:
    HRESULT CreateBackgroundTrigger(BackgroundTriggerType triggerType,
                                   DWORD interval,
                                   IBackgroundTrigger** trigger);
    
    BackgroundTaskInfo CreateTaskInfoFromRegistration(IBackgroundTaskRegistration^ registration);
};

// Push notification service
class PushNotificationService {
private:
    std::wstring m_channelUri;
    PushNotificationCallback m_notificationCallback;
    ComPtr<PushNotificationChannel> m_channel;
    bool m_initialized;
    Windows::Foundation::EventRegistrationToken m_notificationReceivedToken;

public:
    PushNotificationService();
    ~PushNotificationService();
    
    // Initialize push notification service
    HRESULT Initialize();
    
    // Shutdown service
    void Shutdown();
    
    // Create push notification channel
    HRESULT CreateNotificationChannel(std::wstring* channelUri);
    
    // Send local notification
    HRESULT SendLocalNotification(const std::wstring& title,
                                 const std::wstring& content,
                                 const std::wstring& launchArgs = L"");
    
    // Update tile notification
    HRESULT UpdateTileNotification(const std::wstring& content);
    
    // Update badge notification
    HRESULT UpdateBadgeNotification(DWORD badgeNumber);
    
    // Clear all notifications
    HRESULT ClearAllNotifications();
    
    // Check if push notifications are available
    bool IsPushNotificationAvailable() const;
    
    // Set notification callback
    void SetPushNotificationCallback(PushNotificationCallback callback);
    
    // Get current channel URI
    std::wstring GetChannelUri() const;

private:
    HRESULT InitializeNotificationChannel();
    void OnPushNotificationReceived(PushNotificationChannel^ sender,
                                   PushNotificationReceivedEventArgs^ args);
    
    PushNotificationData CreateNotificationDataFromArgs(PushNotificationReceivedEventArgs^ args);
};

// App lifecycle manager
class AppLifecycleManager {
private:
    AppLifecycleState m_currentState;
    LifecycleStateCallback m_stateCallback;
    bool m_initialized;
    Windows::Foundation::EventRegistrationToken m_suspendingToken;
    Windows::Foundation::EventRegistrationToken m_resumingToken;

public:
    AppLifecycleManager();
    ~AppLifecycleManager();
    
    // Initialize lifecycle management
    HRESULT Initialize();
    
    // Shutdown lifecycle management
    void Shutdown();
    
    // Get current app state
    AppLifecycleState GetCurrentState() const;
    
    // Request app suspension
    HRESULT RequestSuspension();
    
    // Resume from suspension
    HRESULT ResumeFromSuspension();
    
    // Handle app activation
    HRESULT HandleActivation(const std::wstring& activationArgs);
    
    // Register for state change notifications
    void SetStateChangeCallback(LifecycleStateCallback callback);
    
    // App state management
    HRESULT SaveAppState();
    HRESULT LoadAppState();
    
    // Check if app can be suspended
    bool CanSuspend() const;

private:
    void OnSuspending(Platform::Object^ sender, SuspendingEventArgs^ args);
    void OnResuming(Platform::Object^ sender, Platform::Object^ args);
    void UpdateAppState(AppLifecycleState newState);
};

// Main system services manager
class SystemServicesManager {
private:
    std::unique_ptr<PhoneDialerService> m_phoneDialer;
    std::unique_ptr<SMSService> m_smsService;
    std::unique_ptr<ContactsService> m_contactsService;
    std::unique_ptr<CalendarService> m_calendarService;
    std::unique_ptr<BackgroundTaskService> m_backgroundTaskService;
    std::unique_ptr<PushNotificationService> m_pushNotificationService;
    std::unique_ptr<AppLifecycleManager> m_lifecycleManager;
    
    bool m_initialized;
    CRITICAL_SECTION m_criticalSection;

public:
    SystemServicesManager();
    ~SystemServicesManager();
    
    // Initialize all system services
    HRESULT Initialize();
    
    // Shutdown all services
    void Shutdown();
    
    // Service accessors
    PhoneDialerService* GetPhoneDialerService();
    SMSService* GetSMSService();
    ContactsService* GetContactsService();
    CalendarService* GetCalendarService();
    BackgroundTaskService* GetBackgroundTaskService();
    PushNotificationService* GetPushNotificationService();
    AppLifecycleManager* GetAppLifecycleManager();
    
    // Check service availability
    bool IsServiceAvailable(SystemServiceType serviceType) const;
    
    // Initialize specific service
    HRESULT InitializeService(SystemServiceType serviceType);
    
    // Shutdown specific service
    void ShutdownService(SystemServiceType serviceType);
    
    // Get service status
    bool IsServiceInitialized(SystemServiceType serviceType) const;

private:
    void CleanupServices();
    std::wstring GetServiceName(SystemServiceType serviceType) const;
};

// System services factory
class SystemServicesFactory {
public:
    static SystemServicesManager* CreateInstance();
    static void DestroyInstance(SystemServicesManager* instance);
    
private:
    SystemServicesFactory() = default;
};

} // namespace Interop
} // namespace CLRNet