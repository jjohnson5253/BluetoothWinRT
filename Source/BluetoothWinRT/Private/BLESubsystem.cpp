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
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>

#include "Windows/HideWindowsPlatformTypes.h"

#pragma warning(pop)

using namespace winrt::Windows::Devices::Bluetooth::Advertisement;

// Internal struct to hold WinRT objects (prevents header pollution)
struct FBLEWatcherData
{
    BluetoothLEAdvertisementWatcher Watcher{ nullptr };
    winrt::event_token ReceivedToken;
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
    
    // Clear previous results if filtering duplicates
    if (bFilterDuplicates)
    {
        DiscoveredDevices.Empty();
    }

    // Create watcher data struct
    auto* WatcherData = new FBLEWatcherData();
    WatcherData->Watcher = BluetoothLEAdvertisementWatcher();
    WatcherPtr = WatcherData;

    // Use Active scanning to get scan response data (includes device names)
    WatcherData->Watcher.ScanningMode(BluetoothLEScanningMode::Active);

    // Subscribe to Received event
    WatcherData->ReceivedToken = WatcherData->Watcher.Received([this](BluetoothLEAdvertisementWatcher const&, BluetoothLEAdvertisementReceivedEventArgs const& args)
    {
        // Extract device info on WinRT thread
        uint64_t address = args.BluetoothAddress();
        int16_t rssi = args.RawSignalStrengthInDBm();
        auto advertisement = args.Advertisement();
        
        // Convert address to hex string
        FString addressStr = FString::Printf(TEXT("%012llX"), address);
        
        // Get device name from advertisement
        FString deviceName;
        auto localName = advertisement.LocalName();
        if (!localName.empty())
        {
            deviceName = FString(localName.c_str());
        }
        else
        {
            deviceName = TEXT("Unknown Device");
        }

        // Get advertised service UUIDs
        TArray<FString> serviceUUIDs;
        auto serviceUuids = advertisement.ServiceUuids();
        for (auto const& uuid : serviceUuids)
        {
            FString uuidStr = FString::Printf(TEXT("%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X"),
                uuid.Data1, uuid.Data2, uuid.Data3,
                uuid.Data4[0], uuid.Data4[1], uuid.Data4[2], uuid.Data4[3],
                uuid.Data4[4], uuid.Data4[5], uuid.Data4[6], uuid.Data4[7]);
            serviceUUIDs.Add(uuidStr);
        }

        // Marshal to game thread
        AsyncTask(ENamedThreads::GameThread, [this, addressStr, deviceName, rssi, serviceUUIDs]()
        {
            // Check if we already have this device
            if (DiscoveredDevices.Contains(addressStr))
            {
                FBLEDeviceInfo& existing = DiscoveredDevices[addressStr];
                bool bNameUpdated = (deviceName != TEXT("Unknown Device") && existing.DeviceName == TEXT("Unknown Device"));
                
                // Update existing entry
                if (bNameUpdated)
                {
                    existing.DeviceName = deviceName;
                }
                existing.RSSI = rssi;
                
                // Broadcast if name was updated (so UI can refresh)
                if (bNameUpdated)
                {
                    UE_LOG(LogTemp, Log, TEXT("BLE Device Name Updated: %s (%s)"), *deviceName, *addressStr);
                    OnDeviceDiscovered.Broadcast(existing);
                }
                return;
            }

            // Create device info struct for new device
            FBLEDeviceInfo DeviceInfo;
            DeviceInfo.DeviceAddress = addressStr;
            DeviceInfo.DeviceName = deviceName;
            DeviceInfo.RSSI = rssi;
            DeviceInfo.ServiceUUIDs = serviceUUIDs;

            // Store in map
            DiscoveredDevices.Add(addressStr, DeviceInfo);

            UE_LOG(LogTemp, Log, TEXT("BLE Device Found: %s (%s) RSSI: %d"), *deviceName, *addressStr, rssi);

            // Broadcast to Blueprint
            OnDeviceDiscovered.Broadcast(DeviceInfo);
        });
    });

    // Subscribe to Stopped event
    WatcherData->StoppedToken = WatcherData->Watcher.Stopped([this](BluetoothLEAdvertisementWatcher const&, BluetoothLEAdvertisementWatcherStoppedEventArgs const&)
    {
        AsyncTask(ENamedThreads::GameThread, [this]()
        {
            bIsScanning = false;
            UE_LOG(LogTemp, Log, TEXT("BLE scan stopped"));
            OnScanStateChanged.Broadcast(false);
        });
    });

    // Start scanning
    WatcherData->Watcher.Start();
    bIsScanning = true;

    UE_LOG(LogTemp, Log, TEXT("BLE scan started"));
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
            WatcherData->Watcher.Received(WatcherData->ReceivedToken);
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
