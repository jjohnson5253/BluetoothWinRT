#include "BluetoothWinRTFunctionLibrary.h"
#include "CoreMinimal.h"

#if PLATFORM_WINDOWS

// Suppress warnings from C++/WinRT headers that conflict with UE build settings
#pragma warning(push)
#pragma warning(disable: 4668) // '_DEBUG' / '_M_ARM64EC' / '_M_IX86' not defined

#include "Windows/AllowWindowsPlatformTypes.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>

#include "Windows/HideWindowsPlatformTypes.h"

#pragma warning(pop)

#endif // PLATFORM_WINDOWS

void UBluetoothWinRTFunctionLibrary::StartConnectToDeviceByAddress(const FString& DeviceAddress)
{
#if PLATFORM_WINDOWS
    // Note: Do NOT call winrt::init_apartment() here.
    // Unreal Engine already initializes COM (MTA). Calling init_apartment() after
    // COM is initialized causes "Cannot change thread mode after it is set" error.

    // Expect address in hex string format "0123456789AB" (no separators)
    std::wstring addressW(*DeviceAddress);

    // Convert hex string to uint64
    uint64_t bluetoothAddress = 0;
    for (wchar_t c : addressW)
    {
        bluetoothAddress <<= 4;
        if (c >= L'0' && c <= L'9') bluetoothAddress |= (c - L'0');
        else if (c >= L'A' && c <= L'F') bluetoothAddress |= (10 + c - L'A');
        else if (c >= L'a' && c <= L'f') bluetoothAddress |= (10 + c - L'a');
    }

    // Asynchronously connect to device (fully qualified to avoid collision with UE's Windows namespace)
    auto op = winrt::Windows::Devices::Bluetooth::BluetoothLEDevice::FromBluetoothAddressAsync(bluetoothAddress);
    op.Completed([DeviceAddress](auto const& asyncInfo, winrt::Windows::Foundation::AsyncStatus asyncStatus)
    {
        if (asyncStatus == winrt::Windows::Foundation::AsyncStatus::Completed)
        {
            auto device = asyncInfo.GetResults();
            if (device)
            {
                UE_LOG(LogTemp, Log, TEXT("Connected to device: %s"), *DeviceAddress);

                // Cycling Power service UUID: 0x1818 -> 00001818-0000-1000-8000-00805f9b34fb
                winrt::guid cyclingPowerGuid{ 0x00001818, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb } };
                
                // Cycling Speed and Cadence service UUID: 0x1816 -> 00001816-0000-1000-8000-00805f9b34fb
                winrt::guid cyclingSpeedCadenceGuid{ 0x00001816, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb } };

                // Try Cycling Power service first
                auto powerServicesOp = device.GetGattServicesForUuidAsync(cyclingPowerGuid);
                powerServicesOp.Completed([DeviceAddress, device, cyclingSpeedCadenceGuid](auto const& svcAsync, winrt::Windows::Foundation::AsyncStatus svcStatus)
                {
                    if (svcStatus == winrt::Windows::Foundation::AsyncStatus::Completed)
                    {
                        auto result = svcAsync.GetResults();
                        if (result.Services().Size() > 0)
                        {
                            auto service = result.Services().GetAt(0);
                            UE_LOG(LogTemp, Log, TEXT("Found Cycling Power service (0x1818) on device: %s"), *DeviceAddress);
                            // Next step: enumerate characteristics for power measurement
                        }
                        else
                        {
                            UE_LOG(LogTemp, Log, TEXT("Cycling Power service not found, trying Cycling Speed and Cadence..."));
                            
                            // Try Cycling Speed and Cadence service
                            auto cscServicesOp = device.GetGattServicesForUuidAsync(cyclingSpeedCadenceGuid);
                            cscServicesOp.Completed([DeviceAddress](auto const& cscAsync, winrt::Windows::Foundation::AsyncStatus cscStatus)
                            {
                                if (cscStatus == winrt::Windows::Foundation::AsyncStatus::Completed)
                                {
                                    auto cscResult = cscAsync.GetResults();
                                    if (cscResult.Services().Size() > 0)
                                    {
                                        auto cscService = cscResult.Services().GetAt(0);
                                        UE_LOG(LogTemp, Log, TEXT("Found Cycling Speed and Cadence service (0x1816) on device: %s"), *DeviceAddress);
                                        // Next step: enumerate characteristics for speed/cadence
                                    }
                                    else
                                    {
                                        UE_LOG(LogTemp, Warning, TEXT("No cycling services found on device: %s"), *DeviceAddress);
                                    }
                                }
                                else
                                {
                                    UE_LOG(LogTemp, Warning, TEXT("GetGattServicesForUuidAsync (CSC) failed for device: %s"), *DeviceAddress);
                                }
                            });
                        }
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("GetGattServicesForUuidAsync (Power) failed for device: %s"), *DeviceAddress);
                    }
                });
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Failed to get BluetoothLEDevice from address: %s"), *DeviceAddress);
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("FromBluetoothAddressAsync failed for: %s"), *DeviceAddress);
        }
    });
#else
    UE_LOG(LogTemp, Warning, TEXT("StartConnectToDeviceByAddress is only implemented on Windows/WinRT"));
#endif
}
