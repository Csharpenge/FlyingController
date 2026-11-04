// Copyright Epic Games, Inc. All Rights Reserved.

#include "FlyingControllerDevice.h"

#include "FlyingControllerEvents.h"
#include "FlyingControllerKeys.h"
#include "FlyingControllerSettings.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "GenericPlatform/InputDeviceRegistry.h"
#include "InputCoreTypes.h"
#include "Misc/App.h"

DEFINE_LOG_CATEGORY_STATIC(LogFlyingController, Log, All);

namespace
{
	/** 兼容模式下的手柄键位（仅在 bEmitLegacyGamepadKeys 打开时使用）。 */
	FName LegacyGamepadKeyForRoll() { return FGamepadKeyNames::RightAnalogX; }
	FName LegacyGamepadKeyForPitch() { return FGamepadKeyNames::RightAnalogY; }
	FName LegacyGamepadKeyForYaw() { return FGamepadKeyNames::LeftAnalogX; }
	FName LegacyGamepadKeyForThrottle() { return FGamepadKeyNames::LeftAnalogY; }

	/** 按钮 usage → 手柄按键；超出范围的忽略。 */
	FName LegacyGamepadKeyForButton(uint16 Usage)
	{
		switch (Usage)
		{
		case 1: return FGamepadKeyNames::FaceButtonBottom;
		case 2: return FGamepadKeyNames::FaceButtonRight;
		case 3: return FGamepadKeyNames::FaceButtonLeft;
		case 4: return FGamepadKeyNames::FaceButtonTop;
		case 5: return FGamepadKeyNames::LeftShoulder;
		case 6: return FGamepadKeyNames::RightShoulder;
		case 7: return FGamepadKeyNames::SpecialRight;
		case 8: return FGamepadKeyNames::LeftThumb;
		default: return NAME_None;
		}
	}

	const TCHAR* CommandTail(const TCHAR* Cmd, const TCHAR* Prefix)
	{
		if (FCString::Strifind(Cmd, Prefix))
		{
			return Cmd + FCString::Strlen(Prefix);
		}
		return Cmd;
	}
}

FFlyingControllerDevice::FFlyingControllerDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
	: MessageHandler(InMessageHandler)
{
	UE_LOG(LogFlyingController, Log, TEXT("FlyingController 设备已创建，开始查找飞行手柄"));
	TryConnect();
}

FFlyingControllerDevice::~FFlyingControllerDevice()
{
	Disconnect();
}

bool FFlyingControllerDevice::TryConnect()
{
	const UFlyingControllerSettings* Settings = GetDefault<UFlyingControllerSettings>();
	if (!Settings || !Settings->bEnabled)
	{
		return false;
	}

	const TArray<FFlyingControllerHidDeviceInfo> Devices = FFlyingControllerHid::EnumerateDevices();
	if (Devices.Num() == 0)
	{
		return false;
	}

	TArray<FFlyingControllerHidDeviceInfo> Candidates;
	for (const FFlyingControllerHidDeviceInfo& Device : Devices)
	{
		if (Settings->VendorId != 0 && Device.VendorId != Settings->VendorId)
		{
			continue;
		}
		if (Settings->ProductId != 0 && Device.ProductId != Settings->ProductId)
		{
			continue;
		}
		if (!Settings->ProductNameContains.IsEmpty() && !Device.ProductName.Contains(Settings->ProductNameContains, ESearchCase::IgnoreCase))
		{
			continue;
		}
		Candidates.Add(Device);
	}

	if (!Candidates.IsValidIndex(Settings->DeviceIndex))
	{
		if (Devices.Num() > 0)
		{
			UE_LOG(LogFlyingController, Warning,
				TEXT("找到 %d 台手柄类设备，但没有匹配当前过滤条件的（VID=%04X PID=%04X 名称含\"%s\"）。可用设备："),
				Devices.Num(), Settings->VendorId, Settings->ProductId, *Settings->ProductNameContains);
			for (const FFlyingControllerHidDeviceInfo& Device : Devices)
			{
				UE_LOG(LogFlyingController, Warning, TEXT("  %s"), *Device.ToString());
			}
		}
		return false;
	}

	const FFlyingControllerHidDeviceInfo& Chosen = Candidates[Settings->DeviceIndex];
	if (!Hid.Open(Chosen))
	{
		return false;
	}

	// 注册到引擎的输入设备映射表，事件才能路由到玩家。
	// 这里固定绑到主用户：本工程是单机训练，手柄应当直接驱动 0 号玩家，
	// 用 GetPlatformUserForNewlyConnectedDevice 可能把设备分到新用户上导致 PIE 收不到输入。
	IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
	DeviceId = Mapper.AllocateNewInputDeviceId();
	PlatformUserId = Mapper.GetPrimaryPlatformUser();

	FInputDeviceDescriptor Descriptor;
	Descriptor.InputDeviceName = FName(TEXT("FlyingController"));
	Descriptor.HardwareDeviceHandle = DeviceId;
	Descriptor.HardwareDeviceIdentifier = FName(TEXT("FlyingController"));
	FInputDeviceRegistry::RegisterDevice(Descriptor);

	Mapper.Internal_MapInputDeviceToUser(DeviceId, PlatformUserId, EInputDeviceConnectionState::Connected);

	bConnected = true;
	TimeSinceConnectAttempt = 0.0f;
	PreviousButtons.Reset();

	// 记录设备信息并广播「设备已连接」。
	ConnectedDeviceInfo = FLyingControllerDeviceInfo();
	ConnectedDeviceInfo.ProductName = Chosen.ProductName;
	ConnectedDeviceInfo.VendorId = static_cast<int32>(Chosen.VendorId);
	ConnectedDeviceInfo.ProductId = static_cast<int32>(Chosen.ProductId);
	ConnectedDeviceInfo.DevicePath = Chosen.DevicePath;
	ConnectedDeviceInfo.bConnected = true;

	UE_LOG(LogFlyingController, Log, TEXT("飞行手柄已接入：%s%s"), *Chosen.ToString(), *Hid.DescribeCaps());
	UE_LOG(LogFlyingController, Log, TEXT("轴映射：Roll=%04X Pitch=%04X Throttle=%04X Yaw=%04X（Usage Page %04X）"),
		Settings->RollUsage, Settings->PitchUsage, Settings->ThrottleUsage, Settings->YawUsage, Settings->AxisUsagePage);

	UE_LOG(LogFlyingController, Log, TEXT("输出键：%s / %s / %s / %s（兼容 Gamepad 输出：%s）"),
		*FlyingControllerKeys::Throttle().ToString(),
		*FlyingControllerKeys::Yaw().ToString(),
		*FlyingControllerKeys::Pitch().ToString(),
		*FlyingControllerKeys::Roll().ToString(),
		Settings->bEmitLegacyGamepadKeys ? TEXT("开") : TEXT("关"));

	UE_LOG(LogFlyingController, Log, TEXT("广播事件 OnDeviceConnected"));
	FFlyingControllerEvents::OnDeviceConnected().Broadcast(ConnectedDeviceInfo);

	return true;
}

void FFlyingControllerDevice::Disconnect()
{
	// 引擎退出时不要做这两件事：
	//   1) Internal_MapInputDeviceToUser(Disconnected) 会回调引擎侧监听者
	//      （InputDebuggingEditor、InputDeviceSubsystem 等），它们此时可能已经析构；
	//   2) 广播自己的事件也会打到已经销毁的监听对象。
	// 之前就是在这里崩的：EXCEPTION_ACCESS_VIOLATION reading 0x138，
	// 栈是 Disconnect → ApplicationCore → InputDebuggingEditor → Engine。
	const bool bEngineShuttingDown = IsEngineExitRequested();

	if (bConnected)
	{
		if (!bEngineShuttingDown)
		{
			IPlatformInputDeviceMapper::Get().Internal_MapInputDeviceToUser(DeviceId, PlatformUserId, EInputDeviceConnectionState::Disconnected);

			ConnectedDeviceInfo.bConnected = false;
			UE_LOG(LogFlyingController, Log, TEXT("飞行手柄已断开（%s），广播事件 OnDeviceDisconnected"), *ConnectedDeviceInfo.ProductName);
			FFlyingControllerEvents::OnDeviceDisconnected().Broadcast(ConnectedDeviceInfo);
		}
		else
		{
			UE_LOG(LogFlyingController, Log, TEXT("引擎关闭中，跳过设备注销与断开事件广播（避免访问已销毁的监听者）"));
		}
	}

	ConnectedDeviceInfo.bConnected = false;
	Hid.Close();
	bConnected = false;
	AxisCalibrations.Reset();
}

bool FFlyingControllerDevice::ReadAxis(int32 Usage, uint32& OutValue, FFlyingControllerAxisRange& OutRange) const
{
	const UFlyingControllerSettings* Settings = GetDefault<UFlyingControllerSettings>();
	const uint16 Page = static_cast<uint16>(Settings->AxisUsagePage);
	return Hid.ReadValue(Page, static_cast<uint16>(Usage), OutValue, OutRange);
}

void FFlyingControllerDevice::UpdateCalibration(uint16 Usage, uint32 RawValue, bool bCenteredAxis)
{
	FFlyingAxisCalibration& Cal = AxisCalibrations.FindOrAdd(Usage);
	const float Raw = static_cast<float>(RawValue);

	if (!Cal.bInitialized)
	{
		Cal.bInitialized = true;
		Cal.Center = Raw;
		Cal.Min = Raw;
		Cal.Max = Raw;
		return;
	}

	// 回中轴的中位只在「接近中位」时缓慢跟随，避免把摇杆的极限位置误当成中位。
	if (bCenteredAxis)
	{
		const float Span = FMath::Max(Cal.Max - Cal.Min, 1.0f);
		if (FMath::Abs(Raw - Cal.Center) < Span * 0.05f)
		{
			Cal.Center = FMath::Lerp(Cal.Center, Raw, 0.02f);
		}
	}

	// 行程只扩展不收缩：用户把杆推到底一次，之后就记住了。
	Cal.Min = FMath::Min(Cal.Min, Raw);
	Cal.Max = FMath::Max(Cal.Max, Raw);
}

void FFlyingControllerDevice::ResetCalibration()
{
	AxisCalibrations.Reset();
	UE_LOG(LogFlyingController, Log, TEXT("轴标定已重置，下一次读数为新的起点"));
}

float FFlyingControllerDevice::NormalizeCentered(uint16 Usage, uint32 RawValue, const FFlyingControllerAxisRange& Range)
{
	UpdateCalibration(Usage, RawValue, /*bCenteredAxis=*/true);

	const FFlyingAxisCalibration& Cal = AxisCalibrations.FindChecked(Usage);
	const float LogicalHalf = FMath::Max((static_cast<float>(Range.LogicalMax) - static_cast<float>(Range.LogicalMin)) * 0.5f, 1.0f);

	// 按方向分别归一化：这台手柄前后行程不对称（实测 27~225，中位 122），
	// 用统一半程会把短的一侧压成 -0.92。两个方向各自除自己的行程后都能到满量程。
	const float Raw = static_cast<float>(RawValue);
	const float PositiveSpan = FMath::Max(Cal.Max - Cal.Center, LogicalHalf * 0.35f);
	const float NegativeSpan = FMath::Max(Cal.Center - Cal.Min, LogicalHalf * 0.35f);

	const float Span = Raw >= Cal.Center ? PositiveSpan : NegativeSpan;
	const float Normalized = (Raw - Cal.Center) / Span;
	return FMath::Clamp(Normalized, -1.0f, 1.0f);
}

float FFlyingControllerDevice::NormalizeThrottle(uint16 Usage, uint32 RawValue, const FFlyingControllerAxisRange& Range)
{
	UpdateCalibration(Usage, RawValue, /*bCenteredAxis=*/false);

	const FFlyingAxisCalibration& Cal = AxisCalibrations.FindChecked(Usage);

	float Min = Cal.Min;
	float Max = Cal.Max;

	// 只防「行程还没学到」的极端情况：学到一点就用实际行程。
	// 之前这里按逻辑量程（0~255）回退，导致刚通电那几秒油门最大只能到 0.78
	// （物理行程只有 4~202），必须推满一次来回才正确，体验很差。
	if (Max - Min < 8.0f)
	{
		Min = static_cast<float>(Range.LogicalMin);
		Max = static_cast<float>(Range.LogicalMax);
	}

	return FMath::Clamp((static_cast<float>(RawValue) - Min) / FMath::Max(Max - Min, 1.0f), 0.0f, 1.0f);
}

void FFlyingControllerDevice::Tick(float DeltaTime)
{
	const UFlyingControllerSettings* Settings = GetDefault<UFlyingControllerSettings>();
	if (!Settings || !Settings->bEnabled)
	{
		return;
	}

	if (!bConnected)
	{
		TimeSinceConnectAttempt += DeltaTime;
		if (TimeSinceConnectAttempt >= Settings->ReconnectInterval)
		{
			TimeSinceConnectAttempt = 0.0f;
			TryConnect();
		}
		return;
	}

	// 一次 Tick 里把积压的报告全部吃掉，只保留最后一帧的轴值。
	int32 ReportsRead = 0;
	while (Hid.PollReport() && ReportsRead < 32)
	{
		++ReportsRead;
	}

	if (!Hid.IsOpen())
	{
		// PollReport 内部检测到掉线（拔线/读错误）：走完整的断开流程，广播断开事件。
		Disconnect();
		return;
	}

	// 一定要注意：没读到新报告时不能更新通道。
	// 刚打开设备时 ReportBuffer 是全 0 的，拿它去标定会把中位记成 0，
	// 之后真实值 122 就会被算成 +0.99（踩过这个坑）。
	if (ReportsRead == 0)
	{
		return;
	}

	UpdateChannels();
}

void FFlyingControllerDevice::UpdateChannels()
{
	const UFlyingControllerSettings* Settings = GetDefault<UFlyingControllerSettings>();

	uint32 RawValue = 0;
	FFlyingControllerAxisRange Range;

	float NewRoll = 0.0f;
	if (ReadAxis(Settings->RollUsage, RawValue, Range))
	{
		NewRoll = NormalizeCentered(static_cast<uint16>(Settings->RollUsage), RawValue, Range);
	}
	else
	{
		static bool bWarnedRoll = false;
		if (!bWarnedRoll)
		{
			bWarnedRoll = true;
			UE_LOG(LogFlyingController, Warning, TEXT("设备没有 usage %04X（横滚），按 0 处理；检查 Axes|Mapping 设置。"), Settings->RollUsage);
		}
	}

	float NewPitch = 0.0f;
	if (ReadAxis(Settings->PitchUsage, RawValue, Range))
	{
		NewPitch = NormalizeCentered(static_cast<uint16>(Settings->PitchUsage), RawValue, Range);
	}

	float NewYaw = 0.0f;
	if (ReadAxis(Settings->YawUsage, RawValue, Range))
	{
		NewYaw = NormalizeCentered(static_cast<uint16>(Settings->YawUsage), RawValue, Range);
	}

	float NewThrottle = 0.5f;
	if (ReadAxis(Settings->ThrottleUsage, RawValue, Range))
	{
		NewThrottle = NormalizeThrottle(static_cast<uint16>(Settings->ThrottleUsage), RawValue, Range);
		if (Settings->bThrottleCentered)
		{
			// 自回中油门：中心 = 50%。
			NewThrottle = FMath::Clamp(NormalizeCentered(static_cast<uint16>(Settings->ThrottleUsage), RawValue, Range) * 0.5f + 0.5f, 0.0f, 1.0f);
		}
	}

	// 反向
	ChannelRoll = Settings->bInvertRoll ? -NewRoll : NewRoll;
	ChannelPitch = Settings->bInvertPitch ? -NewPitch : NewPitch;
	ChannelYaw = Settings->bInvertYaw ? -NewYaw : NewYaw;
	ChannelThrottle = Settings->bInvertThrottle ? 1.0f - NewThrottle : NewThrottle;

	// 死区 + 指数曲线（只作用于三根回中轴，油门不动）
	const float DeadZone = Settings->DeadZone;
	const float Expo = Settings->Expo;

	auto ShapeAxis = [DeadZone, Expo](float Value)
	{
		const float AbsValue = FMath::Abs(Value);
		if (AbsValue <= DeadZone)
		{
			return 0.0f;
		}

		float Scaled = (AbsValue - DeadZone) / FMath::Max(1.0f - DeadZone, KINDA_SMALL_NUMBER);
		Scaled = FMath::Pow(Scaled, 1.0f + Expo * 2.0f);
		return FMath::Sign(Value) * FMath::Clamp(Scaled, 0.0f, 1.0f);
	};

	ChannelRoll = ShapeAxis(ChannelRoll);
	ChannelPitch = ShapeAxis(ChannelPitch);
	ChannelYaw = ShapeAxis(ChannelYaw);

	const bool bWantChannelLog = Settings->bLogChannelValues || bConsoleDebug;
	const bool bWantRawLog = Settings->bLogRawValues || bRawDump;

	if (bWantChannelLog || bWantRawLog)
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - LastChannelLogTime > 0.2)
		{
			LastChannelLogTime = Now;

			if (bWantChannelLog)
			{
				UE_LOG(LogFlyingController, Log, TEXT("Throttle=%.3f Yaw=%+.3f Pitch=%+.3f Roll=%+.3f"),
					ChannelThrottle, ChannelYaw, ChannelPitch, ChannelRoll);
			}

			if (bWantRawLog)
			{
				UE_LOG(LogFlyingController, Log, TEXT("RAW %s"), *Hid.GetRawValuesString());
			}
		}
	}
}

void FFlyingControllerDevice::EmitInput()
{
	if (!bConnected)
	{
		return;
	}

	// 轴：每帧发，保证 Enhanced Input 拿到连续值。
	// 用插件自己的键（FlyingController_*），编辑器不认识这些键，所以不会出现
	// 「一推杆编辑器视口就跟着动」的问题；只有绑定了这些键的 InputAction 会响应。
	MessageHandler->OnControllerAnalog(FlyingControllerKeys::Throttle().GetFName(), PlatformUserId, DeviceId, ChannelThrottle * 2.0f - 1.0f);
	MessageHandler->OnControllerAnalog(FlyingControllerKeys::Yaw().GetFName(), PlatformUserId, DeviceId, ChannelYaw);
	MessageHandler->OnControllerAnalog(FlyingControllerKeys::Pitch().GetFName(), PlatformUserId, DeviceId, ChannelPitch);
	MessageHandler->OnControllerAnalog(FlyingControllerKeys::Roll().GetFName(), PlatformUserId, DeviceId, ChannelRoll);

	// 可选兼容：同时发一份 Gamepad_* 事件（默认关闭，打开后编辑器会重新把手柄当游戏手柄）。
	const UFlyingControllerSettings* Settings = GetDefault<UFlyingControllerSettings>();
	if (Settings && Settings->bEmitLegacyGamepadKeys)
	{
		MessageHandler->OnControllerAnalog(LegacyGamepadKeyForThrottle(), PlatformUserId, DeviceId, ChannelThrottle * 2.0f - 1.0f);
		MessageHandler->OnControllerAnalog(LegacyGamepadKeyForYaw(), PlatformUserId, DeviceId, ChannelYaw);
		MessageHandler->OnControllerAnalog(LegacyGamepadKeyForPitch(), PlatformUserId, DeviceId, ChannelPitch);
		MessageHandler->OnControllerAnalog(LegacyGamepadKeyForRoll(), PlatformUserId, DeviceId, ChannelRoll);
	}

	// 按键：只发边沿。
	TSet<uint16> CurrentButtons;
	for (uint16 Usage : PressedButtons)
	{
		CurrentButtons.Add(Usage);
	}

	for (uint16 Usage : CurrentButtons)
	{
		if (!PreviousButtons.Contains(Usage))
		{
			if (const FKey& Key = FlyingControllerKeys::Button(Usage); Key.IsValid())
			{
				MessageHandler->OnControllerButtonPressed(Key.GetFName(), PlatformUserId, DeviceId, /*bIsRepeat=*/false);
			}

			const FName LegacyKey = LegacyGamepadKeyForButton(Usage);
			if (Settings && Settings->bEmitLegacyGamepadKeys && LegacyKey != NAME_None)
			{
				MessageHandler->OnControllerButtonPressed(LegacyKey, PlatformUserId, DeviceId, false);
			}
		}
	}

	for (uint16 Usage : PreviousButtons)
	{
		if (!CurrentButtons.Contains(Usage))
		{
			if (const FKey& Key = FlyingControllerKeys::Button(Usage); Key.IsValid())
			{
				MessageHandler->OnControllerButtonReleased(Key.GetFName(), PlatformUserId, DeviceId, /*bIsRepeat=*/false);
			}

			const FName LegacyKey = LegacyGamepadKeyForButton(Usage);
			if (Settings && Settings->bEmitLegacyGamepadKeys && LegacyKey != NAME_None)
			{
				MessageHandler->OnControllerButtonReleased(LegacyKey, PlatformUserId, DeviceId, false);
			}
		}
	}

	PreviousButtons = MoveTemp(CurrentButtons);
}

void FFlyingControllerDevice::SendControllerEvents()
{
	if (!bConnected)
	{
		return;
	}

	Hid.ReadPressedButtons(PressedButtons);
	EmitInput();
}

bool FFlyingControllerDevice::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	const TCHAR* Tail = CommandTail(Cmd, TEXT("FlyingController."));

	if (FCString::Stricmp(Tail, TEXT("Status")) == 0)
	{
		Ar.Logf(TEXT("FlyingController: connected=%s device=\"%s\" throttle=%.3f yaw=%+.3f pitch=%+.3f roll=%+.3f"),
			bConnected ? TEXT("true") : TEXT("false"),
			bConnected ? *Hid.GetInfo().ProductName : TEXT("(none)"),
			ChannelThrottle, ChannelYaw, ChannelPitch, ChannelRoll);
		return true;
	}

	if (FCString::Stricmp(Tail, TEXT("ListDevices")) == 0 || FCString::Stricmp(Tail, TEXT("List")) == 0)
	{
		const TArray<FFlyingControllerHidDeviceInfo> Devices = FFlyingControllerHid::EnumerateDevices();
		Ar.Logf(TEXT("FlyingController: 发现 %d 台手柄类设备"), Devices.Num());
		for (const FFlyingControllerHidDeviceInfo& Device : Devices)
		{
			Ar.Logf(TEXT("  %s"), *Device.ToString());
		}
		return true;
	}

	if (FCString::Stricmp(Tail, TEXT("Reconnect")) == 0)
	{
		Disconnect();
		const bool bOk = TryConnect();
		Ar.Logf(TEXT("FlyingController: 重新连接 %s"), bOk ? TEXT("成功") : TEXT("失败"));
		return true;
	}

	if (FCString::Strnicmp(Tail, TEXT("Debug"), 5) == 0 || FCString::Strnicmp(Tail, TEXT("Log"), 3) == 0)
	{
		const TCHAR* NumPtr = Tail + (FCString::Strnicmp(Tail, TEXT("Debug"), 5) == 0 ? 5 : 3);
		while (*NumPtr == TEXT(' '))
		{
			++NumPtr;
		}

		const int32 Value = *NumPtr ? FCString::Atoi(NumPtr) : 1;
		bConsoleDebug = Value != 0;
		Ar.Logf(TEXT("FlyingController: 通道日志 %s"), bConsoleDebug ? TEXT("已开启") : TEXT("已关闭"));
		return true;
	}

	if (FCString::Strnicmp(Tail, TEXT("Raw"), 3) == 0)
	{
		const TCHAR* NumPtr = Tail + 3;
		while (*NumPtr == TEXT(' '))
		{
			++NumPtr;
		}

		const int32 Value = *NumPtr ? FCString::Atoi(NumPtr) : 1;
		bRawDump = Value != 0;
		Ar.Logf(TEXT("FlyingController: 原始轴值日志 %s"), bRawDump ? TEXT("已开启") : TEXT("已关闭"));
		return true;
	}

	if (FCString::Stricmp(Tail, TEXT("Calibrate")) == 0 || FCString::Stricmp(Tail, TEXT("Recenter")) == 0)
	{
		ResetCalibration();
		Ar.Logf(TEXT("FlyingController: 标定已重置（保持摇杆回中几秒，再各自推到底一次即可重新学习行程）"));
		return true;
	}

	return false;
}
