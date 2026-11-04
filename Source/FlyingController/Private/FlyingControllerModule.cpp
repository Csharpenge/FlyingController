// Copyright Epic Games, Inc. All Rights Reserved.

#include "FlyingControllerModule.h"

#include "FlyingControllerDevice.h"
#include "FlyingControllerKeys.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogFlyingControllerModule, Log, All);

void FFlyingControllerModule::StartupModule()
{
	// 基类会把 this 注册成 "InputDevice" 的 modular feature，引擎据此创建设备。
	IInputDeviceModule::StartupModule();

	// 注册插件专用的输入键（FlyingController_*），Enhanced Input 才能在键位列表里选到。
	FlyingControllerKeys::RegisterKeys();

	// 自检：确认键真的进了 EKeys 注册表，并且归类正确（否则键位选择器会因为分类
	// 未注册而把它们塞进 Keyboard 分类，看起来像"没注册"）。
	TArray<FKey> AllKeys;
	EKeys::GetAllKeys(AllKeys);

	int32 FoundKeys = 0;
	for (const FKey& Key : AllKeys)
	{
		if (Key.GetMenuCategory() == FlyingControllerKeys::CategoryName)
		{
			++FoundKeys;
		}
	}

	UE_LOG(LogFlyingControllerModule, Log, TEXT("FlyingController 插件已加载：注册 %d 个轴键 + %d 个按键键，EKeys 中匹配分类的键 %d 个，分类显示名「%s」"),
		FlyingControllerKeys::AxisKeys().Num(),
		FlyingControllerKeys::ButtonCount(),
		FoundKeys,
		*EKeys::GetMenuCategoryDisplayName(FlyingControllerKeys::CategoryName).ToString());
}

void FFlyingControllerModule::ShutdownModule()
{
	FlyingControllerKeys::UnregisterKeys();
	IInputDeviceModule::ShutdownModule();
}

TSharedPtr<IInputDevice> FFlyingControllerModule::CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
	return MakeShared<FFlyingControllerDevice>(InMessageHandler);
}

TSharedPtr<IInputDevice> FFlyingControllerModule::CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler, FInputDeviceCreationParameters InParameters)
{
	// 目前只支持作为主输入设备；副设备（如本地多人）以后再扩展。
	if (InParameters.bInitAsPrimaryDevice)
	{
		return CreateInputDevice(InMessageHandler);
	}

	return nullptr;
}

IMPLEMENT_MODULE(FFlyingControllerModule, FlyingController);
