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

UCLASS()
class BLUETOOTHWINRT_API UBluetoothWinRTFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Connect to a device by Bluetooth address (hex string, no separators). Windows only.
    UFUNCTION(BlueprintCallable, Category = "Bluetooth|WinRT")
    static void StartConnectToDeviceByAddress(const FString& DeviceAddress);
};
