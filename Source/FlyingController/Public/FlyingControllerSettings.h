// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FlyingControllerSettings.generated.h"

/**
 * FlyingController 插件的工程设置（Project Settings → Plugins → Flying Controller）。
 *
 * FS-SM600 默认按 Mode 2 遥控器理解：左摇杆 = 油门 + 偏航，右摇杆 = 俯仰 + 横滚。
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Flying Controller"))
class UFlyingControllerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UFlyingControllerSettings();

	virtual FName GetCategoryName() const override;

	//~ 设备选择

	/** 关闭后插件不枚举 HID、不发输入事件。 */
	UPROPERTY(Config, EditAnywhere, Category = "Device")
	bool bEnabled = true;

	/** 只接受该 VID 的设备；0 = 不限制。 */
	UPROPERTY(Config, EditAnywhere, Category = "Device")
	int32 VendorId = 0;

	/** 只接受该 PID 的设备；0 = 不限制。 */
	UPROPERTY(Config, EditAnywhere, Category = "Device")
	int32 ProductId = 0;

	/** 产品名包含该子串才匹配（忽略大小写）；空 = 不限制。 */
	UPROPERTY(Config, EditAnywhere, Category = "Device")
	FString ProductNameContains;

	/** 匹配到多台设备时取第几台。 */
	UPROPERTY(Config, EditAnywhere, Category = "Device", meta = (ClampMin = "0"))
	int32 DeviceIndex = 0;

	/** 设备断开后重试间隔（秒）。 */
	UPROPERTY(Config, EditAnywhere, Category = "Device", meta = (ClampMin = "0.2"))
	float ReconnectInterval = 2.0f;

	//~ 轴映射（HID usage）

	/** 轴所在的 HID Usage Page，飞行手柄通常是 0x01（Generic Desktop）。 */
	UPROPERTY(Config, EditAnywhere, Category = "Axes|Mapping")
	int32 AxisUsagePage = 0x01;

	/** 横滚（右摇杆左右），默认 0x30 = X。 */
	UPROPERTY(Config, EditAnywhere, Category = "Axes|Mapping")
	int32 RollUsage = 0x30;

	/** 俯仰（右摇杆前后），默认 0x31 = Y。 */
	UPROPERTY(Config, EditAnywhere, Category = "Axes|Mapping")
	int32 PitchUsage = 0x31;

	/** 油门（左摇杆前后），默认 0x32 = Z。 */
	UPROPERTY(Config, EditAnywhere, Category = "Axes|Mapping")
	int32 ThrottleUsage = 0x32;

	/** 偏航（左摇杆左右）。FS-SM600 实测为 0x34 = Ry；通用手柄多为 0x35 = Rz。 */
	UPROPERTY(Config, EditAnywhere, Category = "Axes|Mapping")
	int32 YawUsage = 0x34;

	//~ 手感

	/** 摇杆死区（0~1）。 */
	UPROPERTY(Config, EditAnywhere, Category = "Axes|Tuning", meta = (ClampMin = "0", ClampMax = "0.9"))
	float DeadZone = 0.04f;

	/** 指数曲线强度：0 = 线性，1 = 很强。用于让小舵量更细腻。 */
	UPROPERTY(Config, EditAnywhere, Category = "Axes|Tuning", meta = (ClampMin = "0", ClampMax = "1"))
	float Expo = 0.0f;

	/** 三轴反向开关。 */
	UPROPERTY(Config, EditAnywhere, Category = "Axes|Tuning")
	bool bInvertRoll = false;

	UPROPERTY(Config, EditAnywhere, Category = "Axes|Tuning")
	bool bInvertPitch = false;

	UPROPERTY(Config, EditAnywhere, Category = "Axes|Tuning")
	bool bInvertThrottle = false;

	UPROPERTY(Config, EditAnywhere, Category = "Axes|Tuning")
	bool bInvertYaw = false;

	/** 油门是否按「中点为 50%」处理（自回中的油门）。FS-SM600 是棘轮油门，保持 false。 */
	UPROPERTY(Config, EditAnywhere, Category = "Axes|Tuning")
	bool bThrottleCentered = false;

	//~ 输出

	/**
	 * 兼容模式：除了插件自己的 FlyingController_* 键之外，再发一份 Gamepad_* 事件。
	 *
	 * 默认关闭。打开后编辑器会重新把这些输入当成手柄（视口/UI 导航会跟着动），
	 * 只有在需要复用现成的 Gamepad 绑定、或暂时不想改 Enhanced Input 资产时才开。
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Output")
	bool bEmitLegacyGamepadKeys = false;

	//~ 调试

	/** 每帧把四个通道的原始/归一化值打到日志（仅调试用，正式打包请关闭）。 */
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bLogChannelValues = false;

	/** 把每个轴的 HID 原始值打到日志，用于现场确认「哪根杆对应哪个 usage」。 */
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bLogRawValues = false;
};
