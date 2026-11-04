// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FlyingControllerTypes.generated.h"

/** 一台飞行手柄的描述，连接/断开事件都会带上它。 */
USTRUCT(BlueprintType)
struct FLYINGCONTROLLER_API FLyingControllerDeviceInfo
{
	GENERATED_BODY()

	/** 设备产品名，例如 "RC Simulator - G4.5+G5+G5.5+G6 Controller"。 */
	UPROPERTY(BlueprintReadOnly, Category = "FlyingController")
	FString ProductName;

	UPROPERTY(BlueprintReadOnly, Category = "FlyingController")
	int32 VendorId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FlyingController")
	int32 ProductId = 0;

	/** HID 设备接口路径，用于区分同型号的多台设备。 */
	UPROPERTY(BlueprintReadOnly, Category = "FlyingController")
	FString DevicePath;

	/** 事件发生时的连接状态：连接事件为 true，断开事件为 false。 */
	UPROPERTY(BlueprintReadOnly, Category = "FlyingController")
	bool bConnected = false;
};
