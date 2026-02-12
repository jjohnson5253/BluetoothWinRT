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
    
    // Clear discovered devices at the start of each scan
    // This ensures we only show currently active devices
    DiscoveredDevices.Empty();
    
    // Create watcher data struct
    auto* WatcherData = new FBLEWatcherData();
    
    // Use AQS selector to find all Bluetooth LE devices (paired and unpaired)
    // Protocol ID for Bluetooth LE: {bb7bb05e-5972-42b5-94fc-76eaa7084d49}
    winrt::hstring selector = L"System.Devices.Aep.ProtocolId:=\"{bb7bb05e-5972-42b5-94fc-76eaa7084d49}\"";
    
    // Request additional properties including signal strength
    winrt::Windows::Foundation::Collections::IVector<winrt::hstring> requestedProperties = winrt::single_threaded_vector<winrt::hstring>();
    requestedProperties.Append(L"System.Devices.Aep.SignalStrength");
    requestedProperties.Append(L"System.Devices.Aep.IsConnected");
    requestedProperties.Append(L"System.Devices.Aep.Bluetooth.Le.IsConnectable");
    
    // Create watcher with AssociationEndpoint kind to discover all BLE devices
    WatcherData->Watcher = DeviceInformation::CreateWatcher(
        selector, 
        requestedProperties, 
        DeviceInformationKind::AssociationEndpoint);
    WatcherPtr = WatcherData;

    // Subscribe to Added event (new device discovered)
    WatcherData->AddedToken = WatcherData->Watcher.Added([this](DeviceWatcher const&, DeviceInformation const& deviceInfo)
    {
        // Extract device info on WinRT thread
        auto deviceId = deviceInfo.Id();
        auto deviceName = deviceInfo.Name();
        
        // Get BT address from ID
        // Format: "BluetoothLE#BluetoothLE<local_adapter_address>-<remote_device_address>"
        // We want the REMOTE device address (after the dash)
        std::wstring idStr = deviceId.c_str();
        FString addressStr;
        
        // Find the dash that separates local adapter from remote device
        size_t dashPos = idStr.rfind(L"-");
        if (dashPos != std::wstring::npos && dashPos + 1 < idStr.length())
        {
            // Extract the remote device address (everything after the last dash)
            std::wstring remoteAddr = idStr.substr(dashPos + 1);
            addressStr = FString(remoteAddr.c_str());
        }
        
        if (addressStr.IsEmpty())
        {
            // Fallback: use full device ID
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

        // Skip devices with no signal (RSSI -100 means cached but not present)
        if (rssi <= -100)
        {
            return;
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
        
        // Extract remote device address from ID (after the last dash)
        std::wstring idStr = deviceId.c_str();
        FString addressStr;
        
        size_t dashPos = idStr.rfind(L"-");
        if (dashPos != std::wstring::npos && dashPos + 1 < idStr.length())
        {
            std::wstring remoteAddr = idStr.substr(dashPos + 1);
            addressStr = FString(remoteAddr.c_str());
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

        // To get the updated name, we need to query the device info asynchronously
        FString capturedAddress = addressStr;
        int32 capturedRssi = rssi;
        
        try
        {
            // Get updated device info to retrieve the name (must match watcher's DeviceInformationKind)
            winrt::Windows::Foundation::Collections::IVector<winrt::hstring> props = winrt::single_threaded_vector<winrt::hstring>();
            props.Append(L"System.Devices.Aep.SignalStrength");
            auto asyncOp = DeviceInformation::CreateFromIdAsync(deviceId, props, DeviceInformationKind::AssociationEndpoint);
            asyncOp.Completed([this, capturedAddress, capturedRssi](auto&& asyncInfo, auto&& asyncStatus)
            {
                if (asyncStatus != winrt::Windows::Foundation::AsyncStatus::Completed)
                {
                    return;
                }
                
                auto deviceInfo = asyncInfo.GetResults();
                if (!deviceInfo)
                {
                    return;
                }
                
                auto deviceName = deviceInfo.Name();
                FString name = FString(deviceName.c_str());
                
                // Marshal to game thread
                Async(EAsyncExecution::TaskGraphMainThread, [this, capturedAddress, name, capturedRssi]()
                {
                    if (DiscoveredDevices.Contains(capturedAddress))
                    {
                        FBLEDeviceInfo& existing = DiscoveredDevices[capturedAddress];
                        
                        // Update name if we got a real name and current is Unknown
                        if (!name.IsEmpty() && name != TEXT("Unknown Device") && existing.DeviceName == TEXT("Unknown Device"))
                        {
                            existing.DeviceName = name;
                            UE_LOG(LogTemp, Log, TEXT("BLE Device Name Updated: %s (%s)"), *name, *capturedAddress);
                            OnDeviceDiscovered.Broadcast(existing);
                        }
                        
                        // Always update RSSI
                        if (capturedRssi > -100)
                        {
                            existing.RSSI = capturedRssi;
                        }
                    }
                });
            });
        }
        catch (...)
        {
            // If async query fails, just update RSSI
            Async(EAsyncExecution::TaskGraphMainThread, [this, capturedAddress, capturedRssi]()
            {
                if (DiscoveredDevices.Contains(capturedAddress) && capturedRssi > -100)
                {
                    DiscoveredDevices[capturedAddress].RSSI = capturedRssi;
                }
            });
        }
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
