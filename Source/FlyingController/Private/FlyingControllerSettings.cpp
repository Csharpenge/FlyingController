// Copyright Epic Games, Inc. All Rights Reserved.

#include "FlyingControllerSettings.h"

UFlyingControllerSettings::UFlyingControllerSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("Flying Controller");
}

FName UFlyingControllerSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}
