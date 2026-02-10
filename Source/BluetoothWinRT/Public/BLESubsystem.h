// BLE Scanning Subsystem for Unreal Engine (Windows WinRT)
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BluetoothWinRTFunctionLibrary.h"
#include "BLESubsystem.generated.h"

// Delegate fired when a new BLE device is discovered during scan
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBLEDeviceDiscovered, const FBLEDeviceInfo&, DeviceInfo);

// Delegate fired when scan state changes
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBLEScanStateChanged, bool, bIsScanning);

/**
 * Game Instance Subsystem for BLE device scanning.
 * Access via GetGameInstance()->GetSubsystem<UBLESubsystem>()
 * or from Blueprints using "Get Game Instance" -> "Get Subsystem" (BLESubsystem)
 */
UCLASS()
class BLUETOOTHWINRT_API UBLESubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // USubsystem interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * Start scanning for BLE devices. Discovered devices will fire OnDeviceDiscovered.
     * @param bFilterDuplicates If true, only broadcasts each device once per scan session
     */
    UFUNCTION(BlueprintCallable, Category = "Bluetooth|WinRT")
    void StartScan(bool bFilterDuplicates = true);

    /**
     * Stop scanning for BLE devices.
     */
    UFUNCTION(BlueprintCallable, Category = "Bluetooth|WinRT")
    void StopScan();

    /**
     * Check if currently scanning.
     */
    UFUNCTION(BlueprintPure, Category = "Bluetooth|WinRT")
    bool IsScanning() const { return bIsScanning; }

    /**
     * Get all devices discovered in the current/last scan session.
     */
    UFUNCTION(BlueprintCallable, Category = "Bluetooth|WinRT")
    TArray<FBLEDeviceInfo> GetDiscoveredDevices() const;

    /**
     * Clear the list of discovered devices.
     */
    UFUNCTION(BlueprintCallable, Category = "Bluetooth|WinRT")
    void ClearDiscoveredDevices();

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Bluetooth|WinRT")
    FOnBLEDeviceDiscovered OnDeviceDiscovered;

    UPROPERTY(BlueprintAssignable, Category = "Bluetooth|WinRT")
    FOnBLEScanStateChanged OnScanStateChanged;

private:
    bool bIsScanning = false;
    bool bFilterDuplicates = true;

    // Map of discovered devices by address (for duplicate filtering and storage)
    TMap<FString, FBLEDeviceInfo> DiscoveredDevices;

    // Platform-specific implementation pointer (opaque - prevents WinRT headers in public header)
    void* WatcherPtr = nullptr;

    void CleanupWatcher();
};
