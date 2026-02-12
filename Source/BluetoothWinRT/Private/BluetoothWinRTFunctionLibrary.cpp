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

// Structure to hold all WinRT objects that must be kept alive for the connection
struct FBLEConnectionData
{
    BluetoothLEDevice Device{ nullptr };
    std::vector<GattDeviceService> Services;
    std::vector<GattCharacteristic> Characteristics;
    std::vector<winrt::event_token> ValueChangedTokens;
    FString DeviceAddress;
    
    ~FBLEConnectionData()
    {
        // Unsubscribe from all notifications
        for (size_t i = 0; i < Characteristics.size() && i < ValueChangedTokens.size(); i++)
        {
            try
            {
                Characteristics[i].ValueChanged(ValueChangedTokens[i]);
            }
            catch (...) {}
        }
        
        // Close services
        for (auto& service : Services)
        {
            try
            {
                service.Close();
            }
            catch (...) {}
        }
        
        // Close device
        if (Device)
        {
            try
            {
                Device.Close();
            }
            catch (...) {}
        }
        
        UE_LOG(LogTemp, Log, TEXT("BLE Connection closed for %s"), *DeviceAddress);
    }
};

// Helper to format characteristic UUID
static FString FormatCharacteristicName(winrt::guid uuid)
{
    // Common characteristic UUIDs
    if (uuid.Data1 == 0x2A37) return TEXT("Heart Rate Measurement");
    if (uuid.Data1 == 0x2A38) return TEXT("Body Sensor Location");
    if (uuid.Data1 == 0x2A39) return TEXT("Heart Rate Control Point");
    if (uuid.Data1 == 0x2A5B) return TEXT("CSC Measurement");
    if (uuid.Data1 == 0x2A5C) return TEXT("CSC Feature");
    if (uuid.Data1 == 0x2A5D) return TEXT("Sensor Location");
    if (uuid.Data1 == 0x2A63) return TEXT("Cycling Power Measurement");
    if (uuid.Data1 == 0x2A64) return TEXT("Cycling Power Vector");
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

// Static member definition
TMap<FString, TSharedPtr<FBLEConnectionData>> UBluetoothWinRTFunctionLibrary::ActiveConnections;

void UBluetoothWinRTFunctionLibrary::StartConnectToDeviceByAddress(const FString& DeviceAddress)
{
#if PLATFORM_WINDOWS
    // Check if already connected
    if (ActiveConnections.Contains(DeviceAddress))
    {
        UE_LOG(LogTemp, Warning, TEXT("Already connected to device: %s"), *DeviceAddress);
        return;
    }
    
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
        }
        
        UE_LOG(LogTemp, Log, TEXT("Connecting to BLE address: 0x%llX"), bluetoothAddress);

        try
        {
            // Synchronously connect to device (we're already on a background thread)
            auto deviceOp = BluetoothLEDevice::FromBluetoothAddressAsync(bluetoothAddress);
            BluetoothLEDevice device = deviceOp.get();
            
            if (!device)
            {
                UE_LOG(LogTemp, Warning, TEXT("Failed to get device from address"));
                return;
            }
            
            UE_LOG(LogTemp, Log, TEXT("Connected to device: %s"), *CapturedAddress);
            
            // Log device info for debugging
            auto deviceId = device.DeviceId();
            UE_LOG(LogTemp, Log, TEXT("Device ID: %s"), deviceId.c_str());
            
            // Check pairing status and pair if needed
            auto deviceInfo = device.DeviceInformation();
            if (deviceInfo && deviceInfo.Pairing())
            {
                bool isPaired = deviceInfo.Pairing().IsPaired();
                UE_LOG(LogTemp, Log, TEXT("Device paired: %s"), isPaired ? TEXT("YES") : TEXT("NO"));
                
                if (!isPaired && deviceInfo.Pairing().CanPair())
                {
                    UE_LOG(LogTemp, Log, TEXT("Attempting to pair with device..."));
                    
                    // Use custom pairing for more control
                    auto customPairing = deviceInfo.Pairing().Custom();
                    if (customPairing)
                    {
                        // Set up pairing requested handler for PIN/confirmation
                        customPairing.PairingRequested([](DeviceInformationCustomPairing const& sender, DevicePairingRequestedEventArgs const& args)
                        {
                            // Accept all pairing types
                            auto pairingKind = args.PairingKind();
                            
                            if (pairingKind == DevicePairingKinds::ConfirmOnly)
                            {
                                UE_LOG(LogTemp, Log, TEXT("Pairing: Confirming..."));
                                args.Accept();
                            }
                            else if (pairingKind == DevicePairingKinds::ConfirmPinMatch)
                            {
                                UE_LOG(LogTemp, Log, TEXT("Pairing: PIN match - %s"), args.Pin().c_str());
                                args.Accept();
                            }
                            else if (pairingKind == DevicePairingKinds::ProvidePin)
                            {
                                UE_LOG(LogTemp, Log, TEXT("Pairing: Providing PIN 0000"));
                                args.Accept(L"0000");
                            }
                            else if (pairingKind == DevicePairingKinds::DisplayPin)
                            {
                                UE_LOG(LogTemp, Log, TEXT("Pairing: Display PIN - %s"), args.Pin().c_str());
                                args.Accept();
                            }
                            else
                            {
                                UE_LOG(LogTemp, Log, TEXT("Pairing: Unknown kind %d, accepting anyway"), static_cast<int>(pairingKind));
                                args.Accept();
                            }
                        });
                        
                        // Attempt pairing with all supported kinds
                        auto pairOp = customPairing.PairAsync(
                            DevicePairingKinds::ConfirmOnly | 
                            DevicePairingKinds::ConfirmPinMatch |
                            DevicePairingKinds::ProvidePin |
                            DevicePairingKinds::DisplayPin);
                        auto pairResult = pairOp.get();
                        
                        if (pairResult.Status() == DevicePairingResultStatus::Paired ||
                            pairResult.Status() == DevicePairingResultStatus::AlreadyPaired)
                        {
                            UE_LOG(LogTemp, Log, TEXT("Pairing successful!"));
                        }
                        else
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Pairing failed (status: %d)"), static_cast<int>(pairResult.Status()));
                        }
                    }
                    else
                    {
                        // Fallback to basic pairing
                        auto pairOp = deviceInfo.Pairing().PairAsync();
                        auto pairResult = pairOp.get();
                        
                        if (pairResult.Status() == DevicePairingResultStatus::Paired ||
                            pairResult.Status() == DevicePairingResultStatus::AlreadyPaired)
                        {
                            UE_LOG(LogTemp, Log, TEXT("Pairing successful!"));
                        }
                        else
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Pairing failed (status: %d)"), static_cast<int>(pairResult.Status()));
                        }
                    }
                }
                else if (!isPaired)
                {
                    UE_LOG(LogTemp, Warning, TEXT("Device cannot be paired (CanPair: false)"));
                }
            }
            
            // Create connection data to persist
            auto ConnectionData = MakeShared<FBLEConnectionData>();
            ConnectionData->Device = device;
            ConnectionData->DeviceAddress = CapturedAddress;
            
            // Get all services - use Uncached to get fresh data after pairing
            UE_LOG(LogTemp, Log, TEXT("Requesting GATT services..."));
            auto servicesOp = device.GetGattServicesAsync(BluetoothCacheMode::Uncached);
            auto servicesResult = servicesOp.get();
            
            UE_LOG(LogTemp, Log, TEXT("GetGattServices status: %d"), static_cast<int>(servicesResult.Status()));
            
            if (servicesResult.Status() != GattCommunicationStatus::Success)
            {
                UE_LOG(LogTemp, Warning, TEXT("Failed to get services (status: %d)"), static_cast<int>(servicesResult.Status()));
                return;
            }
            
            auto services = servicesResult.Services();
            
            UE_LOG(LogTemp, Log, TEXT("Found %d GATT services on device %s:"), services.Size(), *CapturedAddress);
            
            for (uint32_t i = 0; i < services.Size(); i++)
            {
                GattDeviceService service = services.GetAt(i);
                winrt::guid uuid = service.Uuid();
                
                // Store service to keep it alive
                ConnectionData->Services.push_back(service);
                
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
                
                // Get characteristics - wrap in try/catch for services that require pairing
                try
                {
                    auto charOp = service.GetCharacteristicsAsync(BluetoothCacheMode::Uncached);
                    auto charResult = charOp.get();
                    
                    if (charResult.Status() == GattCommunicationStatus::AccessDenied)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("    Access denied to characteristics"));
                        continue;
                    }
                    else if (charResult.Status() == GattCommunicationStatus::ProtocolError)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("    Protocol error accessing characteristics (ProtocolError: 0x%02X)"), 
                            charResult.ProtocolError() ? charResult.ProtocolError().Value() : 0);
                        continue;
                    }
                    else if (charResult.Status() != GattCommunicationStatus::Success)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("    Cannot access characteristics (status: %d)"), static_cast<int>(charResult.Status()));
                        continue;
                    }
                    
                    auto characteristics = charResult.Characteristics();
                    UE_LOG(LogTemp, Log, TEXT("    Found %d characteristics"), characteristics.Size());
                    
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
                            
                            try
                            {
                                // Enable notifications/indications FIRST before subscribing
                                GattClientCharacteristicConfigurationDescriptorValue cccdValue = 
                                    supportsNotify ? GattClientCharacteristicConfigurationDescriptorValue::Notify 
                                                  : GattClientCharacteristicConfigurationDescriptorValue::Indicate;
                                
                                auto writeOp = characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(cccdValue);
                                auto writeResult = writeOp.get();
                                
                                if (writeResult == GattCommunicationStatus::Success)
                                {
                                    // Only subscribe to events if CCCD write succeeded
                                    ConnectionData->Characteristics.push_back(characteristic);
                                    
                                    FString capturedCharName = charName;
                                    auto token = characteristic.ValueChanged([CapturedAddress, capturedCharName](GattCharacteristic const& sender, GattValueChangedEventArgs const& args)
                                    {
                                        IBuffer value = args.CharacteristicValue();
                                        FString hexData = BufferToHexString(value);
                                        
                                        UE_LOG(LogTemp, Log, TEXT("[%s] %s: %s"), 
                                            *CapturedAddress, *capturedCharName, *hexData);
                                    });
                                    
                                    ConnectionData->ValueChangedTokens.push_back(token);
                                    UE_LOG(LogTemp, Log, TEXT("    Successfully subscribed to %s"), *capturedCharName);
                                }
                                else if (writeResult == GattCommunicationStatus::AccessDenied)
                                {
                                    UE_LOG(LogTemp, Warning, TEXT("    Access denied for %s - requires pairing"), *charName);
                                }
                                else if (writeResult == GattCommunicationStatus::ProtocolError)
                                {
                                    UE_LOG(LogTemp, Warning, TEXT("    Protocol error for %s"), *charName);
                                }
                                else
                                {
                                    UE_LOG(LogTemp, Warning, TEXT("    Failed to subscribe to %s (status: %d)"), 
                                        *charName, static_cast<int>(writeResult));
                                }
                            }
                            catch (winrt::hresult_error const& charEx)
                            {
                                UE_LOG(LogTemp, Warning, TEXT("    Exception for %s (0x%08X) - skipping"), 
                                    *charName, charEx.code().value);
                            }
                            catch (...)
                            {
                                UE_LOG(LogTemp, Warning, TEXT("    Unknown exception for %s - skipping"), *charName);
                            }
                        }
                    }
                }
                catch (winrt::hresult_error const& svcEx)
                {
                    UE_LOG(LogTemp, Warning, TEXT("    Service access error (0x%08X) - skipping"), svcEx.code().value);
                }
                catch (...)
                {
                    UE_LOG(LogTemp, Warning, TEXT("    Unknown service error - skipping"));
                }
            }
            
            // Store connection on game thread (even if some subscriptions failed)
            Async(EAsyncExecution::TaskGraphMainThread, [CapturedAddress, ConnectionData]()
            {
                ActiveConnections.Add(CapturedAddress, ConnectionData);
                UE_LOG(LogTemp, Log, TEXT("Connection established and stored for %s (%d characteristics subscribed)"), 
                    *CapturedAddress, ConnectionData->Characteristics.size());
            });
        }
        catch (winrt::hresult_error const& ex)
        {
            UE_LOG(LogTemp, Error, TEXT("WinRT error connecting to device: 0x%08X"), ex.code().value);
        }
        catch (...)
        {
            UE_LOG(LogTemp, Error, TEXT("Unknown error connecting to device"));
        }
    });
#else
    UE_LOG(LogTemp, Warning, TEXT("StartConnectToDeviceByAddress is only implemented on Windows/WinRT"));
#endif
}

void UBluetoothWinRTFunctionLibrary::DisconnectFromDevice(const FString& DeviceAddress)
{
    if (ActiveConnections.Contains(DeviceAddress))
    {
        ActiveConnections.Remove(DeviceAddress);
        UE_LOG(LogTemp, Log, TEXT("Disconnected from device: %s"), *DeviceAddress);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("No active connection for device: %s"), *DeviceAddress);
    }
}

void UBluetoothWinRTFunctionLibrary::DisconnectAll()
{
    int32 count = ActiveConnections.Num();
    ActiveConnections.Empty();
    UE_LOG(LogTemp, Log, TEXT("Disconnected from %d devices"), count);
}
