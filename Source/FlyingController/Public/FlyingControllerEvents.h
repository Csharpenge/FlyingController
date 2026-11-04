// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FlyingControllerTypes.h"

/**
 * 飞行手柄的连接 / 断开回调（C++ 层）。
 *
 * 用法（任意 C++ 侧，不必依赖插件模块已加载）：
 *
 *   FFlyingControllerEvents::OnDeviceConnected().AddUObject(this, &UMyClass::HandleDeviceConnected);
 *   FFlyingControllerEvents::OnDeviceDisconnected().AddUObject(this, &UMyClass::HandleDeviceDisconnected);
 *
 * 记得在对象销毁时 Remove（或改用 UFlyingControllerSubsystem 的蓝图事件，
 * 它会自动处理生命周期）。
 *
 * 注意：事件都在游戏线程（设备 Tick 所在线程）广播。
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnFlyingControllerDeviceConnected, const FLyingControllerDeviceInfo& /*DeviceInfo*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnFlyingControllerDeviceDisconnected, const FLyingControllerDeviceInfo& /*DeviceInfo*/);

struct FLYINGCONTROLLER_API FFlyingControllerEvents
{
	/** 检测到设备连接时广播。 */
	static FOnFlyingControllerDeviceConnected& OnDeviceConnected();

	/** 检测到设备断开（拔线、读错误、手动 Reconnect）时广播。 */
	static FOnFlyingControllerDeviceDisconnected& OnDeviceDisconnected();

	/**
	 * 当前是否已连接（实时值，不依赖事件）。
	 *
	 * 注意：如果你在设备已经连上之后才订阅事件，是收不到那次「连接」的——
	 * 用这个函数查初始状态。
	 */
	static bool IsConnected();

	/** 当前（或最近一次）设备信息。 */
	static FLyingControllerDeviceInfo GetDeviceInfo();
};
