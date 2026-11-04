// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IInputDeviceModule.h"

/**
 * FlyingController 插件模块。
 *
 * 作为一个 IInputDeviceModule 注册到引擎：启动时把自身登记为 "InputDevice" 的
 * modular feature，引擎创建输入设备时回调 CreateInputDevice。
 */
class FFlyingControllerModule final : public IInputDeviceModule
{
public:
	//~ Begin IInputDeviceModule interface
	virtual TSharedPtr<IInputDevice> CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override;
	virtual TSharedPtr<IInputDevice> CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler, FInputDeviceCreationParameters InParameters) override;
	//~ End IInputDeviceModule interface

	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface interface
};
