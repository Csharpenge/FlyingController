// Copyright Epic Games, Inc. All Rights Reserved.

#include "FlyingControllerState.h"

namespace
{
	FLyingControllerInputState GInputState;
}

const FLyingControllerInputState& FlyingControllerState::Get()
{
	return GInputState;
}

FLyingControllerInputState& FlyingControllerState::GetMutable()
{
	return GInputState;
}
