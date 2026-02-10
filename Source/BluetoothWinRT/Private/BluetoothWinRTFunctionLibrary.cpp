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
    winrt::init_apartment();

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

                // FTMS service UUID: 00001826-0000-1000-8000-00805f9b34fb
                winrt::guid ftmsGuid{ 0x00001826, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb } };
                auto servicesOp = device.GetGattServicesForUuidAsync(ftmsGuid);
                servicesOp.Completed([DeviceAddress](auto const& svcAsync, winrt::Windows::Foundation::AsyncStatus svcStatus)
                {
                    if (svcStatus == winrt::Windows::Foundation::AsyncStatus::Completed)
                    {
                        auto result = svcAsync.GetResults();
                        if (result.Services().Size() > 0)
                        {
                            auto service = result.Services().GetAt(0);
                            UE_LOG(LogTemp, Log, TEXT("Found FTMS service on device: %s"), *DeviceAddress);
                            // Next step: enumerate characteristics
                        }
                        else
                        {
                            UE_LOG(LogTemp, Warning, TEXT("FTMS service not found on device: %s"), *DeviceAddress);
                        }
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("GetGattServicesForUuidAsync failed for device: %s"), *DeviceAddress);
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
