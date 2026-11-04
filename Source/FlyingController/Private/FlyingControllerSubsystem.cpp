// Copyright Epic Games, Inc. All Rights Reserved.

#include "FlyingControllerSubsystem.h"

#include "FlyingControllerEvents.h"
#include "FlyingControllerState.h"

void UFlyingControllerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ConnectedHandle = FFlyingControllerEvents::OnDeviceConnected().AddUObject(this, &UFlyingControllerSubsystem::HandleDeviceConnected);
	DisconnectedHandle = FFlyingControllerEvents::OnDeviceDisconnected().AddUObject(this, &UFlyingControllerSubsystem::HandleDeviceDisconnected);

	// 手柄往往在 GameInstance（本子系统）创建之前就连上了，那一次「连接」事件收不到；
	// 这里按当前实时状态补广播一次，保证蓝图里 OnDeviceConnected 的初始状态是对的。
	const FLyingControllerInputState& State = FlyingControllerState::Get();
	if (State.bConnected)
	{
		UE_LOG(LogTemp, Log, TEXT("FlyingControllerSubsystem: 初始化时手柄已连接（%s），补广播连接事件"), *State.Device.ProductName);
		OnDeviceConnected.Broadcast(State.Device);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("FlyingControllerSubsystem: 初始化时手柄未连接"));
	}
}

void UFlyingControllerSubsystem::Deinitialize()
{
	FFlyingControllerEvents::OnDeviceConnected().Remove(ConnectedHandle);
	FFlyingControllerEvents::OnDeviceDisconnected().Remove(DisconnectedHandle);

	Super::Deinitialize();
}

bool UFlyingControllerSubsystem::IsDeviceConnected() const
{
	return FlyingControllerState::Get().bConnected;
}

FLyingControllerDeviceInfo UFlyingControllerSubsystem::GetConnectedDevice() const
{
	return FlyingControllerState::Get().Device;
}

void UFlyingControllerSubsystem::HandleDeviceConnected(const FLyingControllerDeviceInfo& DeviceInfo)
{
	UE_LOG(LogTemp, Log, TEXT("FlyingControllerSubsystem: 收到连接事件（%s），广播给蓝图"), *DeviceInfo.ProductName);
	OnDeviceConnected.Broadcast(DeviceInfo);
}

void UFlyingControllerSubsystem::HandleDeviceDisconnected(const FLyingControllerDeviceInfo& DeviceInfo)
{
	UE_LOG(LogTemp, Log, TEXT("FlyingControllerSubsystem: 收到断开事件（%s），广播给蓝图"), *DeviceInfo.ProductName);
	OnDeviceDisconnected.Broadcast(DeviceInfo);
}
