// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class SWindow;
class FAutoConsoleCommand;
class FToolBarBuilder;

/**
 * 编辑器侧模块：在关卡编辑器工具栏加一个「手柄」按钮，点开实时摇杆面板（浮动窗口）。
 */
class FFlyingControllerEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();

	/** 把「手柄」按钮插到关卡编辑器主工具栏（Content 按钮之后）。 */
	void AddToolbarButton(FToolBarBuilder& Builder);

	/** 打开（或前置）摇杆状态窗口。 */
	void OpenMonitorWindow();

	/** 延迟自动打开（仅 -FlyingControllerAutoOpenMonitor，用于自动化验证）。 */
	bool AutoOpenMonitorTicker(float DeltaTime);

	TSharedPtr<SWindow> MonitorWindow;
	TUniquePtr<FAutoConsoleCommand> OpenMonitorCommand;
};
