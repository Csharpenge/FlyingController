// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IInputDevice.h"
#include "FlyingControllerHid.h"
#include "FlyingControllerTypes.h"

/**
 * 单个轴的标定值。
 *
 * 实测 FS-SM600：静息中位是 122（不是 127.5），油门物理行程只有 4~200（不是 0~255）。
 * 所以不能直接按 HID 逻辑范围归一化，否则中位会漂、油门永远到不了 100%。
 */
struct FFlyingAxisCalibration
{
	bool bInitialized = false;

	/** 回中轴的中位（首次读数 / Recenter 时采样）。 */
	float Center = 127.5f;

	/** 观测到的最小/最大值，用于确定实际行程。 */
	float Min = 0.0f;
	float Max = 255.0f;
};

/**
 * 把 FS-SM600 的 HID 输入翻译成引擎能识别的 Gamepad 轴/按键事件。
 *
 * 为什么映射到 Gamepad_* 而不是自定义 FKey：这样 Enhanced Input、UMG、
 * 现有输入模板都能直接绑，不需要额外资产生成；同时手柄/键盘鼠标可并存。
 */
class FFlyingControllerDevice : public IInputDevice
{
public:
	explicit FFlyingControllerDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler);
	virtual ~FFlyingControllerDevice() override;

	//~ Begin IInputDevice interface
	virtual void Tick(float DeltaTime) override;
	virtual void SendControllerEvents() override;
	virtual void SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override { MessageHandler = InMessageHandler; }
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	virtual void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override {}
	virtual void SetChannelValues(int32 ControllerId, const FForceFeedbackValues& Values) override {}
	virtual bool IsGamepadAttached() const override { return bConnected; }
	virtual EInputDeviceThreadAffinity GetThreadAffinity() const override { return EInputDeviceThreadAffinity::InputThreadSafe; }
	//~ End IInputDevice interface

private:
	/** 尝试找到并打开设备；成功时把设备注册到输入设备映射表。 */
	bool TryConnect();

	/** 断开并释放设备。 */
	void Disconnect();

	/** 把这一帧的 HID 状态换算成四个通道。 */
	void UpdateChannels();

	/** 发轴事件 + 按键边沿事件。 */
	void EmitInput();

	/** 用标定值把回中轴归一化到 [-1,1]。 */
	float NormalizeCentered(uint16 Usage, uint32 RawValue, const FFlyingControllerAxisRange& Range);

	/** 用标定值把油门归一化到 [0,1]。 */
	float NormalizeThrottle(uint16 Usage, uint32 RawValue, const FFlyingControllerAxisRange& Range);

	/** 更新单个轴的标定（首次读数定中位，之后扩展 Min/Max）。 */
	void UpdateCalibration(uint16 Usage, uint32 RawValue, bool bCenteredAxis);

	/** 清空所有标定，下一次读数时重新采样。 */
	void ResetCalibration();

	/** 读单个轴，缺失时返回 false 并把 OutValue 置 0。 */
	bool ReadAxis(int32 Usage, uint32& OutValue, FFlyingControllerAxisRange& OutRange) const;

	TSharedRef<FGenericApplicationMessageHandler> MessageHandler;

	FFlyingControllerHid Hid;
	TArray<uint16> PressedButtons;
	TSet<uint16> PreviousButtons;

	bool bConnected = false;
	float TimeSinceConnectAttempt = 0.0f;

	FInputDeviceId DeviceId;
	FPlatformUserId PlatformUserId;

	// 归一化后的四个通道
	float ChannelThrottle = 0.0f;
	float ChannelYaw = 0.0f;
	float ChannelPitch = 0.0f;
	float ChannelRoll = 0.0f;

	// 最近一次发出的值，避免每帧重复发同值（轴仍然每帧发，便于 UI 读取）
	float LastPostedThrottle = -999.0f;
	float LastPostedYaw = -999.0f;
	float LastPostedPitch = -999.0f;
	float LastPostedRoll = -999.0f;

	/** 控制台 FlyingController.Debug 1 打开的实时通道日志（优先于配置项）。 */
	bool bConsoleDebug = false;

	/** 控制台 FlyingController.Raw 1 打开的原始轴值日志。 */
	bool bRawDump = false;

	/** 通道日志节流用，避免每帧刷屏。 */
	double LastChannelLogTime = 0.0;

	/** usage → 标定值。 */
	TMap<uint16, FFlyingAxisCalibration> AxisCalibrations;

	/** 当前（或最近一次）连接的设备信息，广播事件时带上它。 */
	FLyingControllerDeviceInfo ConnectedDeviceInfo;
};
