#include "BluetoothWinRTFunctionLibrary.h"
#include "CoreMinimal.h"
#include "Async/Async.h"

#if PLATFORM_WINDOWS

#pragma warning(push)
#pragma warning(disable: 4668)

#include "Windows/AllowWindowsPlatformTypes.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>

#include "Windows/HideWindowsPlatformTypes.h"

#pragma warning(pop)

using namespace winrt;
using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Devices::Bluetooth;
using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;
#endif

void UBluetoothWinRTFunctionLibrary::StartConnectToDeviceByAddress(const FString& DeviceAddress)
{
#if PLATFORM_WINDOWS
    // Capture address for async task
    FString CapturedAddress = DeviceAddress;
    
    // Run WinRT code on a background thread to avoid COM apartment conflicts
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [CapturedAddress]()
    {
        // Initialize COM apartment for this background thread
        winrt::init_apartment();

        // Log the input address for debugging
        UE_LOG(LogTemp, Log, TEXT("Parsing BLE address: %s"), *CapturedAddress);

        // Convert address string to raw hex (remove colons if present)
        // Accepts formats: "f0:95:b4:3f:2b:0d" or "f095b43f2b0d"
        
        // Convert string to unsigned long long (skip non-hex characters like ':')
        unsigned long long bluetoothAddress = 0;
        for (int32 i = 0; i < CapturedAddress.Len(); i++)
        {
            TCHAR c = CapturedAddress[i];
            if (c >= TEXT('0') && c <= TEXT('9'))
            {
                bluetoothAddress <<= 4;
                bluetoothAddress |= (c - TEXT('0'));
            }
            else if (c >= TEXT('A') && c <= TEXT('F'))
            {
                bluetoothAddress <<= 4;
                bluetoothAddress |= (10 + c - TEXT('A'));
            }
            else if (c >= TEXT('a') && c <= TEXT('f'))
            {
                bluetoothAddress <<= 4;
                bluetoothAddress |= (10 + c - TEXT('a'));
            }
            // Skip colons and other non-hex characters
        }
        
        UE_LOG(LogTemp, Log, TEXT("Connecting to BLE address: 0x%llX"), bluetoothAddress);

        // Asynchronously connect to device
        auto op = BluetoothLEDevice::FromBluetoothAddressAsync(bluetoothAddress);
        op.Completed([CapturedAddress](auto const& asyncInfo, auto const& asyncStatus)
        {
            if (asyncStatus == winrt::Windows::Foundation::AsyncStatus::Completed)
            {
                BluetoothLEDevice device = asyncInfo.GetResults();
                if (device)
                {
                    UE_LOG(LogTemp, Log, TEXT("Connected to device: %s"), *CapturedAddress);

                    // Find FTMS service (Fitness Machine Service) UUID: 00001826-0000-1000-8000-00805f9b34fb
                    winrt::guid ftmsServiceUuid{ 0x00001826, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb } };
                    auto servicesOp = device.GetGattServicesForUuidAsync(ftmsServiceUuid);
                    servicesOp.Completed([CapturedAddress](auto const& svcAsync, auto const& svcStatus)
                    {
                        if (svcStatus == winrt::Windows::Foundation::AsyncStatus::Completed)
                        {
                            auto result = svcAsync.GetResults();
                            if (result.Services().Size() > 0)
                            {
                                GattDeviceService service = result.Services().GetAt(0);
                                UE_LOG(LogTemp, Log, TEXT("Found FTMS service"));

                                // Here you would find characteristics to read/write. This minimal example stops here.
                            }
                            else
                            {
                                UE_LOG(LogTemp, Warning, TEXT("FTMS service not found"));
                            }
                        }
                    });
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("Failed to get device from address"));
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Failed to connect to device async"));
            }
        });
    });
#else
    UE_LOG(LogTemp, Warning, TEXT("StartConnectToDeviceByAddress is only implemented on Windows/WinRT"));
#endif
}
