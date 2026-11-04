// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FlyingControllerTypes.h"

/**
 * 飞行手柄的实时状态快照，供编辑器面板 / 调试 UI 读取。
 *
 * 由设备每次 Tick 写入，只在游戏线程读写（编辑器面板的 Tick 也在游戏线程）。
 * 这是「只读展示」用的，游戏逻辑请用 Enhanced Input 或 FFlyingControllerEvents。
 */
struct FLYINGCONTROLLER_API FLyingControllerInputState
{
	/** 是否已连接。 */
	bool bConnected = false;

	/** 设备信息（连接时填充）。 */
	FLyingControllerDeviceInfo Device;

	/** 归一化后的四通道：Throttle 0~1，其余 -1~1。 */
	float Throttle = 0.0f;
	float Yaw = 0.0f;
	float Pitch = 0.0f;
	float Roll = 0.0f;

	/** 原始轴值：HID usage → 读数（已按 HID 逻辑范围归一化到 -1~1 或 0~1 之外保留原始值）。 */
	TMap<uint16, uint32> RawAxes;

	/** 当前按下的按钮 usage。 */
	TArray<uint16> PressedButtons;

	/** 最近一次更新的时间戳（秒）。 */
	double LastUpdateTime = 0.0;
};

namespace FlyingControllerState
{
	/** 读取当前状态（只读）。 */
	FLYINGCONTROLLER_API const FLyingControllerInputState& Get();

	/** 写入状态，仅供 FFlyingControllerDevice 使用。 */
	FLYINGCONTROLLER_API FLyingControllerInputState& GetMutable();
}
