// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

/**
 * 插件注册的专用输入键。
 *
 * 为什么不复用 Gamepad_*：编辑器会把 Gamepad 键当成手柄输入（视口导航、UI 导航），
 * 飞行手柄一推杆编辑器就跟着动。这里改用独立命名的键，Slate/编辑器完全不认识，
 * 只有绑定到这些键的 Enhanced Input 才会响应。
 *
 * 键在模块启动时通过 EKeys::AddKey 注册，编辑器里新建 InputAction 时可以在
 * 「Flying Controller」分类下选到。
 */
namespace FlyingControllerKeys
{
	/** 油门，0~1。 */
	FLYINGCONTROLLER_API const FKey& Throttle();

	/** 偏航，-1~1。 */
	FLYINGCONTROLLER_API const FKey& Yaw();

	/** 俯仰，-1~1。 */
	FLYINGCONTROLLER_API const FKey& Pitch();

	/** 横滚，-1~1。 */
	FLYINGCONTROLLER_API const FKey& Roll();

	/** 第 Index 个按键（1~8）；越界返回无效键。 */
	FLYINGCONTROLLER_API const FKey& Button(int32 Index);

	/** 按键数量（固定 8）。 */
	FLYINGCONTROLLER_API int32 ButtonCount();

	/** 四个轴键，顺序为 Throttle / Yaw / Pitch / Roll。 */
	FLYINGCONTROLLER_API const TArray<FKey>& AxisKeys();

	/** 注册 / 注销所有键，由模块启动与关闭调用。 */
	FLYINGCONTROLLER_API void RegisterKeys();
	FLYINGCONTROLLER_API void UnregisterKeys();

	/** 键在编辑器键位选择器里的分类名。 */
	static const FName CategoryName = FName(TEXT("Flying Controller"));
}
