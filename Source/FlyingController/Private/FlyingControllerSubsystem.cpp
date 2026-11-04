// Copyright Epic Games, Inc. All Rights Reserved.

#include "FlyingControllerSubsystem.h"

#include "FlyingControllerEvents.h"

void UFlyingControllerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ConnectedHandle = FFlyingControllerEvents::OnDeviceConnected().AddUObject(this, &UFlyingControllerSubsystem::HandleDeviceConnected);
	DisconnectedHandle = FFlyingControllerEvents::OnDeviceDisconnected().AddUObject(this, &UFlyingControllerSubsystem::HandleDeviceDisconnected);
}

void UFlyingControllerSubsystem::Deinitialize()
{
	FFlyingControllerEvents::OnDeviceConnected().Remove(ConnectedHandle);
	FFlyingControllerEvents::OnDeviceDisconnected().Remove(DisconnectedHandle);

	Super::Deinitialize();
}

void UFlyingControllerSubsystem::HandleDeviceConnected(const FLyingControllerDeviceInfo& DeviceInfo)
{
	bConnected = true;
	ConnectedDevice = DeviceInfo;
	OnDeviceConnected.Broadcast(DeviceInfo);
}

void UFlyingControllerSubsystem::HandleDeviceDisconnected(const FLyingControllerDeviceInfo& DeviceInfo)
{
	bConnected = false;
	ConnectedDevice = DeviceInfo;
	OnDeviceDisconnected.Broadcast(DeviceInfo);
}
