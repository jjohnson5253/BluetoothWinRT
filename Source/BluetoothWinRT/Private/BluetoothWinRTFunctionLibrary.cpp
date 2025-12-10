#include "BluetoothWinRTFunctionLibrary.h"
#include "CoreMinimal.h"

#if PLATFORM_WINDOWS
#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
using namespace winrt;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
#endif

void UBluetoothWinRTFunctionLibrary::StartConnectToDeviceByAddress(const FString& DeviceAddress)
{
#if PLATFORM_WINDOWS
    init_apartment();

    // Expect address in hex string format "0123456789AB" (no separators)
    std::wstring addressW = std::wstring(*DeviceAddress);

    // Convert string to unsigned long long
    unsigned long long bluetoothAddress = 0;
    for (wchar_t c : addressW)
    {
        bluetoothAddress <<= 4;
        if (c >= L'0' && c <= L'9') bluetoothAddress |= (c - L'0');
        else if (c >= L'A' && c <= L'F') bluetoothAddress |= (10 + c - L'A');
        else if (c >= L'a' && c <= L'f') bluetoothAddress |= (10 + c - L'a');
    }

    // Asynchronously connect to device
    auto op = BluetoothLEDevice::FromBluetoothAddressAsync(bluetoothAddress);
    op.Completed([=](auto const& asyncInfo, auto const& asyncStatus)
    {
        if (asyncStatus == Windows::Foundation::AsyncStatus::Completed)
        {
            BluetoothLEDevice device = asyncInfo.GetResults();
            if (device)
            {
                UE_LOG(LogTemp, Log, TEXT("Connected to device: %s"), *DeviceAddress);

                // Find FTMS service (Fitness Machine Service) UUID: 00001826-0000-1000-8000-00805f9b34fb
                auto servicesOp = device.GetGattServicesForUuidAsync({ 0x00001826, 0x0000, 0x1000, 0x8000, 0x00805f9b34fb });
                servicesOp.Completed([=](auto const& svcAsync, auto const& svcStatus)
                {
                    if (svcStatus == Windows::Foundation::AsyncStatus::Completed)
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
#else
    UE_LOG(LogTemp, Warning, TEXT("StartConnectToDeviceByAddress is only implemented on Windows/WinRT"));
#endif
}
