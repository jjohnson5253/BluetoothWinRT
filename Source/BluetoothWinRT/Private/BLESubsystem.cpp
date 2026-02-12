// BLE Scanning Subsystem implementation
#include "BLESubsystem.h"
#include "Async/Async.h"

#if PLATFORM_WINDOWS

#pragma warning(push)
#pragma warning(disable: 4668)

#include "Windows/AllowWindowsPlatformTypes.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include "Windows/HideWindowsPlatformTypes.h"

#pragma warning(pop)

using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Devices::Bluetooth;

// Internal struct to hold WinRT objects (prevents header pollution)
struct FBLEWatcherData
{
    DeviceWatcher Watcher{ nullptr };
    winrt::event_token AddedToken;
    winrt::event_token UpdatedToken;
    winrt::event_token RemovedToken;
    winrt::event_token StoppedToken;
};

#endif // PLATFORM_WINDOWS

void UBLESubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("BLESubsystem initialized"));
}

void UBLESubsystem::Deinitialize()
{
    StopScan();
    CleanupWatcher();
    Super::Deinitialize();
    UE_LOG(LogTemp, Log, TEXT("BLESubsystem deinitialized"));
}

void UBLESubsystem::StartScan(bool bInFilterDuplicates)
{
#if PLATFORM_WINDOWS
    if (bIsScanning)
    {
        UE_LOG(LogTemp, Warning, TEXT("BLE scan already in progress"));
        return;
    }

    bFilterDuplicates = bInFilterDuplicates;
    
    // Don't clear previous results - keep device names we've learned
    // DiscoveredDevices map persists across scans to remember names
    
    // Create watcher data struct
    auto* WatcherData = new FBLEWatcherData();
    
    // Create device selector for Bluetooth LE devices
    auto selector = BluetoothLEDevice::GetDeviceSelector();
    WatcherData->Watcher = DeviceInformation::CreateWatcher(selector);
    WatcherPtr = WatcherData;

    // Subscribe to Added event (new device discovered)
    WatcherData->AddedToken = WatcherData->Watcher.Added([this](DeviceWatcher const&, DeviceInformation const& deviceInfo)
    {
        // Extract device info on WinRT thread
        auto deviceId = deviceInfo.Id();
        auto deviceName = deviceInfo.Name();
        
        // Get BT address from ID (format: "BluetoothLE#BluetoothLE<address>-<service>")
        std::wstring idStr = deviceId.c_str();
        size_t pos = idStr.find(L"BluetoothLE");
        FString addressStr;
        
        if (pos != std::wstring::npos)
        {
            // Extract the hex address portion
            size_t addrStart = pos + 11; // Skip "BluetoothLE"
            size_t addrEnd = idStr.find(L"-", addrStart);
            if (addrEnd != std::wstring::npos)
            {
                std::wstring addrPart = idStr.substr(addrStart, addrEnd - addrStart);
                addressStr = FString(addrPart.c_str());
            }
        }
        
        if (addressStr.IsEmpty())
        {
            // Fallback: use device ID
            addressStr = FString(deviceId.c_str());
        }
        
        FString name = FString(deviceName.c_str());
        if (name.IsEmpty())
        {
            name = TEXT("Unknown Device");
        }

        // Get signal strength (RSSI) if available
        int32 rssi = -100;
        auto properties = deviceInfo.Properties();
        if (properties.HasKey(L"System.Devices.Aep.SignalStrength"))
        {
            auto rssiObj = properties.Lookup(L"System.Devices.Aep.SignalStrength");
            if (rssiObj)
            {
                rssi = winrt::unbox_value<int32_t>(rssiObj);
            }
        }

        // Marshal to game thread
        Async(EAsyncExecution::TaskGraphMainThread, [this, addressStr, name, rssi]()
        {
            // Check if we already have this device
            if (DiscoveredDevices.Contains(addressStr))
            {
                FBLEDeviceInfo& existing = DiscoveredDevices[addressStr];
                bool bNameUpdated = (name != TEXT("Unknown Device") && existing.DeviceName == TEXT("Unknown Device"));
                
                // Update existing entry
                if (bNameUpdated)
                {
                    existing.DeviceName = name;
                    UE_LOG(LogTemp, Log, TEXT("BLE Device Name Updated: %s (%s)"), *name, *addressStr);
                }
                existing.RSSI = rssi;
                
                // Always broadcast so UI updates
                OnDeviceDiscovered.Broadcast(existing);
                return;
            }

            // Create device info struct for new device
            FBLEDeviceInfo DeviceInfo;
            DeviceInfo.DeviceAddress = addressStr;
            DeviceInfo.DeviceName = name;
            DeviceInfo.RSSI = rssi;

            // Store in map
            DiscoveredDevices.Add(addressStr, DeviceInfo);

            UE_LOG(LogTemp, Log, TEXT("BLE Device Found: %s (%s) RSSI: %d"), *name, *addressStr, rssi);

            // Broadcast to Blueprint
            OnDeviceDiscovered.Broadcast(DeviceInfo);
        });
    });

    // Subscribe to Updated event (device properties changed)
    WatcherData->UpdatedToken = WatcherData->Watcher.Updated([this](DeviceWatcher const&, DeviceInformationUpdate const& updateInfo)
    {
        auto deviceId = updateInfo.Id();
        
        // Extract address from ID
        std::wstring idStr = deviceId.c_str();
        size_t pos = idStr.find(L"BluetoothLE");
        FString addressStr;
        
        if (pos != std::wstring::npos)
        {
            size_t addrStart = pos + 11;
            size_t addrEnd = idStr.find(L"-", addrStart);
            if (addrEnd != std::wstring::npos)
            {
                std::wstring addrPart = idStr.substr(addrStart, addrEnd - addrStart);
                addressStr = FString(addrPart.c_str());
            }
        }
        
        if (addressStr.IsEmpty())
        {
            addressStr = FString(deviceId.c_str());
        }

        // Get updated RSSI if available
        int32 rssi = -100;
        auto properties = updateInfo.Properties();
        if (properties.HasKey(L"System.Devices.Aep.SignalStrength"))
        {
            auto rssiObj = properties.Lookup(L"System.Devices.Aep.SignalStrength");
            if (rssiObj)
            {
                rssi = winrt::unbox_value<int32_t>(rssiObj);
            }
        }

        // Marshal to game thread
        Async(EAsyncExecution::TaskGraphMainThread, [this, addressStr, rssi]()
        {
            if (DiscoveredDevices.Contains(addressStr))
            {
                FBLEDeviceInfo& existing = DiscoveredDevices[addressStr];
                existing.RSSI = rssi;
                // Silently update RSSI without broadcasting (too frequent)
            }
        });
    });

    // Subscribe to Stopped event
    WatcherData->StoppedToken = WatcherData->Watcher.Stopped([this](DeviceWatcher const&, winrt::Windows::Foundation::IInspectable const&)
    {
        Async(EAsyncExecution::TaskGraphMainThread, [this]()
        {
            bIsScanning = false;
            UE_LOG(LogTemp, Log, TEXT("BLE scan stopped"));
            OnScanStateChanged.Broadcast(false);
        });
    });

    // Start scanning
    WatcherData->Watcher.Start();
    bIsScanning = true;

    UE_LOG(LogTemp, Log, TEXT("BLE scan started (using DeviceWatcher API)"));
    OnScanStateChanged.Broadcast(true);

#else
    UE_LOG(LogTemp, Warning, TEXT("BLE scanning is only supported on Windows"));
#endif
}

void UBLESubsystem::StopScan()
{
#if PLATFORM_WINDOWS
    if (!bIsScanning || !WatcherPtr)
    {
        return;
    }

    auto* WatcherData = static_cast<FBLEWatcherData*>(WatcherPtr);
    WatcherData->Watcher.Stop();
    // Note: bIsScanning will be set to false in the Stopped callback
#endif
}

void UBLESubsystem::CleanupWatcher()
{
#if PLATFORM_WINDOWS
    if (WatcherPtr)
    {
        auto* WatcherData = static_cast<FBLEWatcherData*>(WatcherPtr);
        
        // Unsubscribe from events
        if (WatcherData->Watcher)
        {
            WatcherData->Watcher.Added(WatcherData->AddedToken);
            WatcherData->Watcher.Updated(WatcherData->UpdatedToken);
            WatcherData->Watcher.Stopped(WatcherData->StoppedToken);
        }

        delete WatcherData;
        WatcherPtr = nullptr;
    }
#endif
}

TArray<FBLEDeviceInfo> UBLESubsystem::GetDiscoveredDevices() const
{
    TArray<FBLEDeviceInfo> Result;
    DiscoveredDevices.GenerateValueArray(Result);
    return Result;
}

TArray<UBLEDeviceObject*> UBLESubsystem::GetDiscoveredDevicesAsObjects()
{
    TArray<UBLEDeviceObject*> Result;
    for (const auto& Pair : DiscoveredDevices)
    {
        Result.Add(UBLEDeviceObject::Create(Pair.Value, this));
    }
    // Sort by RSSI (strongest signal first)
    Result.Sort([](const UBLEDeviceObject& A, const UBLEDeviceObject& B)
    {
        return A.DeviceInfo.RSSI > B.DeviceInfo.RSSI;
    });
    return Result;
}

void UBLESubsystem::ClearDiscoveredDevices()
{
    DiscoveredDevices.Empty();
}
