// Minimal Blueprint-callable function for WinRT BLE
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "BluetoothWinRTFunctionLibrary.generated.h"

// Struct to hold discovered BLE device information
USTRUCT(BlueprintType)
struct BLUETOOTHWINRT_API FBLEDeviceInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Bluetooth")
    FString DeviceName;

    UPROPERTY(BlueprintReadOnly, Category = "Bluetooth")
    FString DeviceAddress;  // Hex string e.g., "5434B7E9E436"

    UPROPERTY(BlueprintReadOnly, Category = "Bluetooth")
    int32 RSSI = 0;  // Signal strength in dBm

    UPROPERTY(BlueprintReadOnly, Category = "Bluetooth")
    TArray<FString> ServiceUUIDs;  // Advertised service UUIDs
};

// UObject wrapper for FBLEDeviceInfo (needed for ListView data binding)
UCLASS(BlueprintType)
class BLUETOOTHWINRT_API UBLEDeviceObject : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "Bluetooth")
    FBLEDeviceInfo DeviceInfo;

    // Helper function to create and initialize a device object
    static UBLEDeviceObject* Create(const FBLEDeviceInfo& Info, UObject* Outer)
    {
        UBLEDeviceObject* Obj = NewObject<UBLEDeviceObject>(Outer);
        Obj->DeviceInfo = Info;
        return Obj;
    }
};

// Forward declaration for internal connection data
struct FBLEConnectionData;

UCLASS()
class BLUETOOTHWINRT_API UBluetoothWinRTFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Connect to a device by Bluetooth address (hex string, no separators). Windows only.
    UFUNCTION(BlueprintCallable, Category = "Bluetooth|WinRT")
    static void StartConnectToDeviceByAddress(const FString& DeviceAddress);

    // Disconnect from a device by address
    UFUNCTION(BlueprintCallable, Category = "Bluetooth|WinRT")
    static void DisconnectFromDevice(const FString& DeviceAddress);

    // Disconnect from all connected devices
    UFUNCTION(BlueprintCallable, Category = "Bluetooth|WinRT")
    static void DisconnectAll();

private:
    // Map to store active connections (prevents objects from being destroyed)
    static TMap<FString, TSharedPtr<FBLEConnectionData>> ActiveConnections;
};
