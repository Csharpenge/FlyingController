// Copyright Epic Games, Inc. All Rights Reserved.

#include "FlyingControllerEvents.h"

#include "FlyingControllerState.h"

FOnFlyingControllerDeviceConnected& FFlyingControllerEvents::OnDeviceConnected()
{
	static FOnFlyingControllerDeviceConnected Delegate;
	return Delegate;
}

FOnFlyingControllerDeviceDisconnected& FFlyingControllerEvents::OnDeviceDisconnected()
{
	static FOnFlyingControllerDeviceDisconnected Delegate;
	return Delegate;
}

bool FFlyingControllerEvents::IsConnected()
{
	return FlyingControllerState::Get().bConnected;
}

FLyingControllerDeviceInfo FFlyingControllerEvents::GetDeviceInfo()
{
	return FlyingControllerState::Get().Device;
}
