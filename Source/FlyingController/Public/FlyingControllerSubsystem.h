// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FlyingControllerTypes.h"
#include "FlyingControllerSubsystem.generated.h"

/** 蓝图可绑定的设备连接事件。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFlyingControllerDeviceConnectedBP, const FLyingControllerDeviceInfo&, DeviceInfo);

/** 蓝图可绑定的设备断开事件。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFlyingControllerDeviceDisconnectedBP, const FLyingControllerDeviceInfo&, DeviceInfo);

/**
 * 飞行手柄的连接状态与事件（蓝图可用）。
 *
 * 在蓝图里：Game Instance → Get Subsystem (Flying Controller) → 绑定 OnDeviceConnected /
 * OnDeviceDisconnected，或用 IsDeviceConnected / GetConnectedDevice 查询当前状态。
 *
 * C++ 侧也可以直接订阅 FFlyingControllerEvents（无需经过本子系统）。
 */
UCLASS()
class FLYINGCONTROLLER_API UFlyingControllerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem interface

	/** 检测到手柄连接。 */
	UPROPERTY(BlueprintAssignable, Category = "FlyingController")
	FOnFlyingControllerDeviceConnectedBP OnDeviceConnected;

	/** 检测到手柄断开。 */
	UPROPERTY(BlueprintAssignable, Category = "FlyingController")
	FOnFlyingControllerDeviceDisconnectedBP OnDeviceDisconnected;

	/**
	 * 手柄当前是否连接。
	 *
	 * 读的是运行期实时状态（FlyingControllerState），不是订阅事件时缓存的旧值——
	 * 因为手柄往往在 GameInstance（本子系统）创建之前就连上了，那一次连接事件收不到。
	 */
	UFUNCTION(BlueprintPure, Category = "FlyingController")
	bool IsDeviceConnected() const;

	/** 当前（或最近一次）设备信息。 */
	UFUNCTION(BlueprintPure, Category = "FlyingController")
	FLyingControllerDeviceInfo GetConnectedDevice() const;

private:
	void HandleDeviceConnected(const FLyingControllerDeviceInfo& DeviceInfo);
	void HandleDeviceDisconnected(const FLyingControllerDeviceInfo& DeviceInfo);

	FDelegateHandle ConnectedHandle;
	FDelegateHandle DisconnectedHandle;
};
