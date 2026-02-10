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

                // Get ALL services on the device (no hardcoded UUIDs)
                auto servicesOp = device.GetGattServicesAsync(
                    winrt::Windows::Devices::Bluetooth::BluetoothCacheMode::Uncached);
                servicesOp.Completed([DeviceAddress, device](auto const& svcAsync, winrt::Windows::Foundation::AsyncStatus svcStatus)
                {
                    if (svcStatus == winrt::Windows::Foundation::AsyncStatus::Completed)
                    {
                        auto result = svcAsync.GetResults();
                        auto gattStatus = result.Status();
                        
                        // Log the GATT communication status
                        FString statusStr;
                        switch (gattStatus)
                        {
                            case winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCommunicationStatus::Success:
                                statusStr = TEXT("Success");
                                break;
                            case winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCommunicationStatus::Unreachable:
                                statusStr = TEXT("Unreachable");
                                break;
                            case winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCommunicationStatus::ProtocolError:
                                statusStr = TEXT("ProtocolError");
                                break;
                            case winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCommunicationStatus::AccessDenied:
                                statusStr = TEXT("AccessDenied");
                                break;
                            default:
                                statusStr = TEXT("Unknown");
                                break;
                        }
                        
                        UE_LOG(LogTemp, Log, TEXT("GATT query status: %s"), *statusStr);
                        
                        if (gattStatus == winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCommunicationStatus::Success)
                        {
                            auto services = result.Services();
                            UE_LOG(LogTemp, Log, TEXT("Found %d services on device: %s"), services.Size(), *DeviceAddress);
                            
                            // Enumerate and log all services
                            for (uint32_t i = 0; i < services.Size(); i++)
                            {
                                auto service = services.GetAt(i);
                                auto uuid = service.Uuid();
                                
                                // Format UUID as string
                                FString uuidStr = FString::Printf(TEXT("%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X"),
                                    uuid.Data1, uuid.Data2, uuid.Data3,
                                    uuid.Data4[0], uuid.Data4[1], uuid.Data4[2], uuid.Data4[3],
                                    uuid.Data4[4], uuid.Data4[5], uuid.Data4[6], uuid.Data4[7]);
                                
                                UE_LOG(LogTemp, Log, TEXT("  Service %d: %s"), i, *uuidStr);
                                
                                // Get characteristics for this service
                                auto charsOp = service.GetCharacteristicsAsync(
                                    winrt::Windows::Devices::Bluetooth::BluetoothCacheMode::Uncached);
                                charsOp.Completed([uuidStr, i](auto const& charAsync, winrt::Windows::Foundation::AsyncStatus charStatus)
                                {
                                    if (charStatus == winrt::Windows::Foundation::AsyncStatus::Completed)
                                    {
                                        auto charResult = charAsync.GetResults();
                                        if (charResult.Status() == winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCommunicationStatus::Success)
                                        {
                                            auto characteristics = charResult.Characteristics();
                                            for (uint32_t j = 0; j < characteristics.Size(); j++)
                                            {
                                                auto characteristic = characteristics.GetAt(j);
                                                auto charUuid = characteristic.Uuid();
                                                auto props = characteristic.CharacteristicProperties();
                                                
                                                FString charUuidStr = FString::Printf(TEXT("%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X"),
                                                    charUuid.Data1, charUuid.Data2, charUuid.Data3,
                                                    charUuid.Data4[0], charUuid.Data4[1], charUuid.Data4[2], charUuid.Data4[3],
                                                    charUuid.Data4[4], charUuid.Data4[5], charUuid.Data4[6], charUuid.Data4[7]);
                                                
                                                // Build properties string
                                                FString propsStr;
                                                if ((props & winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristicProperties::Read) != winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristicProperties::None)
                                                    propsStr += TEXT("Read ");
                                                if ((props & winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristicProperties::Write) != winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristicProperties::None)
                                                    propsStr += TEXT("Write ");
                                                if ((props & winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristicProperties::Notify) != winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristicProperties::None)
                                                    propsStr += TEXT("Notify ");
                                                if ((props & winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristicProperties::Indicate) != winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristicProperties::None)
                                                    propsStr += TEXT("Indicate ");
                                                
                                                UE_LOG(LogTemp, Log, TEXT("    Char %d: %s [%s]"), j, *charUuidStr, *propsStr);
                                            }
                                        }
                                    }
                                });
                            }
                        }
                        else if (gattStatus == winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCommunicationStatus::AccessDenied)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Access denied to GATT services. Device may need to be paired first: %s"), *DeviceAddress);
                        }
                        else
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Failed to get services: %s"), *statusStr);
                        }
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("GetGattServicesAsync failed for device: %s"), *DeviceAddress);
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
