#include "BluetoothWinRTFunctionLibrary.h"
#include "CoreMinimal.h"

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
        if (asyncStatus == winrt::Windows::Foundation::AsyncStatus::Completed)
        {
            BluetoothLEDevice device = asyncInfo.GetResults();
            if (device)
            {
                UE_LOG(LogTemp, Log, TEXT("Connected to device: %s"), *DeviceAddress);

                // Find FTMS service (Fitness Machine Service) UUID: 00001826-0000-1000-8000-00805f9b34fb
                winrt::guid ftmsServiceUuid{ 0x00001826, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb } };
                auto servicesOp = device.GetGattServicesForUuidAsync(ftmsServiceUuid);
                servicesOp.Completed([=](auto const& svcAsync, auto const& svcStatus)
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
#else
    UE_LOG(LogTemp, Warning, TEXT("StartConnectToDeviceByAddress is only implemented on Windows/WinRT"));
#endif
}
