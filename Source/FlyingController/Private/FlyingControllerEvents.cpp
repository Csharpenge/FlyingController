// Copyright Epic Games, Inc. All Rights Reserved.

#include "FlyingControllerEvents.h"

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
