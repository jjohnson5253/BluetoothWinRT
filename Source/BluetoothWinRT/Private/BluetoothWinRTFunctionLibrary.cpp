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
#include <winrt/Windows.Storage.Streams.h>

#include "Windows/HideWindowsPlatformTypes.h"

#pragma warning(pop)

using namespace winrt;
using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Devices::Bluetooth;
using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace winrt::Windows::Storage::Streams;

// Helper to format characteristic UUID
static FString FormatCharacteristicName(winrt::guid uuid)
{
    // Common characteristic UUIDs
    if (uuid.Data1 == 0x2A37) return TEXT("Heart Rate Measurement");
    if (uuid.Data1 == 0x2A38) return TEXT("Body Sensor Location");
    if (uuid.Data1 == 0x2A39) return TEXT("Heart Rate Control Point");
    if (uuid.Data1 == 0x2A5B) return TEXT("CSC Measurement");
    if (uuid.Data1 == 0x2A5C) return TEXT("CSC Feature");
    if (uuid.Data1 == 0x2A63) return TEXT("Cycling Power Measurement");
    if (uuid.Data1 == 0x2A65) return TEXT("Cycling Power Feature");
    if (uuid.Data1 == 0x2A66) return TEXT("Cycling Power Control Point");
    if (uuid.Data1 == 0x2ACC) return TEXT("Fitness Machine Feature");
    if (uuid.Data1 == 0x2ACD) return TEXT("Treadmill Data");
    if (uuid.Data1 == 0x2ACE) return TEXT("Cross Trainer Data");
    if (uuid.Data1 == 0x2AD1) return TEXT("Rower Data");
    if (uuid.Data1 == 0x2AD2) return TEXT("Indoor Bike Data");
    if (uuid.Data1 == 0x2AD3) return TEXT("Training Status");
    if (uuid.Data1 == 0x2AD6) return TEXT("Supported Resistance Level Range");
    if (uuid.Data1 == 0x2AD7) return TEXT("Supported Power Range");
    if (uuid.Data1 == 0x2AD8) return TEXT("Fitness Machine Status");
    if (uuid.Data1 == 0x2AD9) return TEXT("Fitness Machine Control Point");
    if (uuid.Data1 == 0x2A19) return TEXT("Battery Level");
    if (uuid.Data1 == 0x2A29) return TEXT("Manufacturer Name");
    if (uuid.Data1 == 0x2A24) return TEXT("Model Number");
    if (uuid.Data1 == 0x2A25) return TEXT("Serial Number");
    if (uuid.Data1 == 0x2A26) return TEXT("Firmware Revision");
    if (uuid.Data1 == 0x2A27) return TEXT("Hardware Revision");
    if (uuid.Data1 == 0x2A28) return TEXT("Software Revision");
    
    return FString::Printf(TEXT("%08X"), uuid.Data1);
}

// Helper to convert IBuffer to hex string for logging
static FString BufferToHexString(IBuffer const& buffer)
{
    if (!buffer || buffer.Length() == 0)
    {
        return TEXT("(empty)");
    }
    
    DataReader reader = DataReader::FromBuffer(buffer);
    FString result;
    for (uint32_t i = 0; i < buffer.Length(); i++)
    {
        uint8_t byte = reader.ReadByte();
        result += FString::Printf(TEXT("%02X "), byte);
    }
    return result;
}

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

                    // Get ALL GATT services on the device
                    auto servicesOp = device.GetGattServicesAsync();
                    servicesOp.Completed([CapturedAddress, device](auto const& svcAsync, auto const& svcStatus)
                    {
                        if (svcStatus == winrt::Windows::Foundation::AsyncStatus::Completed)
                        {
                            auto result = svcAsync.GetResults();
                            auto services = result.Services();
                            
                            UE_LOG(LogTemp, Log, TEXT("Found %d GATT services on device %s:"), services.Size(), *CapturedAddress);
                            
                            for (uint32_t i = 0; i < services.Size(); i++)
                            {
                                GattDeviceService service = services.GetAt(i);
                                winrt::guid uuid = service.Uuid();
                                
                                // Format UUID as string
                                FString uuidStr = FString::Printf(TEXT("%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X"),
                                    uuid.Data1, uuid.Data2, uuid.Data3,
                                    uuid.Data4[0], uuid.Data4[1], uuid.Data4[2], uuid.Data4[3],
                                    uuid.Data4[4], uuid.Data4[5], uuid.Data4[6], uuid.Data4[7]);
                                
                                // Identify common BLE service UUIDs
                                FString serviceName = TEXT("Unknown");
                                if (uuid.Data1 == 0x1800) serviceName = TEXT("Generic Access");
                                else if (uuid.Data1 == 0x1801) serviceName = TEXT("Generic Attribute");
                                else if (uuid.Data1 == 0x180A) serviceName = TEXT("Device Information");
                                else if (uuid.Data1 == 0x180D) serviceName = TEXT("Heart Rate");
                                else if (uuid.Data1 == 0x180F) serviceName = TEXT("Battery Service");
                                else if (uuid.Data1 == 0x1816) serviceName = TEXT("Cycling Speed and Cadence");
                                else if (uuid.Data1 == 0x1818) serviceName = TEXT("Cycling Power");
                                else if (uuid.Data1 == 0x1826) serviceName = TEXT("Fitness Machine (FTMS)");
                                
                                UE_LOG(LogTemp, Log, TEXT("  [%d] %s - %s"), i, *uuidStr, *serviceName);
                                
                                // Get characteristics for this service and subscribe to notifications
                                FString capturedServiceName = serviceName;
                                auto charOp = service.GetCharacteristicsAsync();
                                charOp.Completed([CapturedAddress, capturedServiceName](auto const& charAsync, auto const& charStatus)
                                {
                                    if (charStatus != winrt::Windows::Foundation::AsyncStatus::Completed)
                                    {
                                        return;
                                    }
                                    
                                    auto charResult = charAsync.GetResults();
                                    auto characteristics = charResult.Characteristics();
                                    
                                    for (uint32_t j = 0; j < characteristics.Size(); j++)
                                    {
                                        GattCharacteristic characteristic = characteristics.GetAt(j);
                                        winrt::guid charUuid = characteristic.Uuid();
                                        GattCharacteristicProperties props = characteristic.CharacteristicProperties();
                                        
                                        FString charName = FormatCharacteristicName(charUuid);
                                        
                                        // Check if this characteristic supports notifications or indications
                                        bool supportsNotify = (static_cast<uint32_t>(props) & static_cast<uint32_t>(GattCharacteristicProperties::Notify)) != 0;
                                        bool supportsIndicate = (static_cast<uint32_t>(props) & static_cast<uint32_t>(GattCharacteristicProperties::Indicate)) != 0;
                                        
                                        if (supportsNotify || supportsIndicate)
                                        {
                                            UE_LOG(LogTemp, Log, TEXT("    Subscribing to characteristic: %s (%s)"), 
                                                *charName, supportsNotify ? TEXT("Notify") : TEXT("Indicate"));
                                            
                                            // Subscribe to value changed event
                                            FString capturedCharName = charName;
                                            characteristic.ValueChanged([CapturedAddress, capturedCharName](GattCharacteristic const& sender, GattValueChangedEventArgs const& args)
                                            {
                                                IBuffer value = args.CharacteristicValue();
                                                FString hexData = BufferToHexString(value);
                                                
                                                UE_LOG(LogTemp, Log, TEXT("[%s] %s: %s"), 
                                                    *CapturedAddress, *capturedCharName, *hexData);
                                            });
                                            
                                            // Enable notifications/indications
                                            GattClientCharacteristicConfigurationDescriptorValue cccdValue = 
                                                supportsNotify ? GattClientCharacteristicConfigurationDescriptorValue::Notify 
                                                              : GattClientCharacteristicConfigurationDescriptorValue::Indicate;
                                            
                                            auto writeOp = characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(cccdValue);
                                            writeOp.Completed([capturedCharName](auto const& writeAsync, auto const& writeStatus)
                                            {
                                                if (writeStatus == winrt::Windows::Foundation::AsyncStatus::Completed)
                                                {
                                                    auto writeResult = writeAsync.GetResults();
                                                    if (writeResult == GattCommunicationStatus::Success)
                                                    {
                                                        UE_LOG(LogTemp, Log, TEXT("    Successfully subscribed to %s"), *capturedCharName);
                                                    }
                                                    else
                                                    {
                                                        UE_LOG(LogTemp, Warning, TEXT("    Failed to subscribe to %s (status: %d)"), 
                                                            *capturedCharName, static_cast<int>(writeResult));
                                                    }
                                                }
                                            });
                                        }
                                    }
                                });
                            }
                            
                            if (services.Size() == 0)
                            {
                                UE_LOG(LogTemp, Warning, TEXT("No GATT services found on device"));
                            }
                        }
                        else
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Failed to get GATT services"));
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
