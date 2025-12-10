// Minimal Blueprint-callable function for WinRT BLE
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "BluetoothWinRTFunctionLibrary.generated.h"

UCLASS()
class BLUETOOTHWINRT_API UBluetoothWinRTFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Start a minimal scan and connect to a device by Bluetooth address (as string). Run only on Windows platform.
    UFUNCTION(BlueprintCallable, Category = "Bluetooth|WinRT")
    static void StartConnectToDeviceByAddress(const FString& DeviceAddress);
};
