// Copyright Epic Games, Inc. All Rights Reserved.

#include "FlyingControllerKeys.h"

#define LOCTEXT_NAMESPACE "FlyingControllerKeys"

namespace
{
	constexpr int32 NumButtons = 8;

	const FKey KeyThrottle(TEXT("FlyingController_Throttle"));
	const FKey KeyYaw(TEXT("FlyingController_Yaw"));
	const FKey KeyPitch(TEXT("FlyingController_Pitch"));
	const FKey KeyRoll(TEXT("FlyingController_Roll"));

	const FKey KeyButtons[8] =	{
		FKey(TEXT("FlyingController_Button1")),
		FKey(TEXT("FlyingController_Button2")),
		FKey(TEXT("FlyingController_Button3")),
		FKey(TEXT("FlyingController_Button4")),
		FKey(TEXT("FlyingController_Button5")),
		FKey(TEXT("FlyingController_Button6")),
		FKey(TEXT("FlyingController_Button7")),
		FKey(TEXT("FlyingController_Button8"))
	};

	const FKey InvalidKey;
}

const FKey& FlyingControllerKeys::Throttle() { return KeyThrottle; }
const FKey& FlyingControllerKeys::Yaw() { return KeyYaw; }
const FKey& FlyingControllerKeys::Pitch() { return KeyPitch; }
const FKey& FlyingControllerKeys::Roll() { return KeyRoll; }

const FKey& FlyingControllerKeys::Button(int32 Index)
{
	return (Index >= 1 && Index <= NumButtons) ? KeyButtons[Index - 1] : InvalidKey;
}

int32 FlyingControllerKeys::ButtonCount() { return NumButtons; }

const TArray<FKey>& FlyingControllerKeys::AxisKeys()
{
	static const TArray<FKey> Keys = { KeyThrottle, KeyYaw, KeyPitch, KeyRoll };
	return Keys;
}

void FlyingControllerKeys::RegisterKeys()
{
	// 必须先注册分类显示名：EKeys::GetMenuCategoryDisplayName() 对未知分类会回退成
	// "Keyboard" 的显示名，键就会混进键盘分类里，看起来像"没注册"。
	EKeys::AddMenuCategoryDisplayInfo(CategoryName,
		LOCTEXT("Category", "Flying Controller"),
		FName(TEXT("Input_Gamepad")));

	// 显示名里带上键名（FlyingController_Throttle），因为键位选择器的搜索
	// 只匹配显示名，不带键名的话搜 "FlyingController" 什么都搜不到。
	EKeys::AddKey(FKeyDetails(KeyThrottle,
		LOCTEXT("Throttle", "FlyingController Throttle（油门）"),
		FKeyDetails::Axis1D, CategoryName));

	EKeys::AddKey(FKeyDetails(KeyYaw,
		LOCTEXT("Yaw", "FlyingController Yaw（偏航）"),
		FKeyDetails::Axis1D, CategoryName));

	EKeys::AddKey(FKeyDetails(KeyPitch,
		LOCTEXT("Pitch", "FlyingController Pitch（俯仰）"),
		FKeyDetails::Axis1D, CategoryName));

	EKeys::AddKey(FKeyDetails(KeyRoll,
		LOCTEXT("Roll", "FlyingController Roll（横滚）"),
		FKeyDetails::Axis1D, CategoryName));

	for (int32 Index = 0; Index < NumButtons; ++Index)
	{
		EKeys::AddKey(FKeyDetails(KeyButtons[Index],
			FText::Format(LOCTEXT("Button", "FlyingController Button{0}（按键{0}）"), FText::AsNumber(Index + 1)),
			0, CategoryName));
	}
}

void FlyingControllerKeys::UnregisterKeys()
{
	EKeys::RemoveKeysWithCategory(CategoryName);
}

#undef LOCTEXT_NAMESPACE
