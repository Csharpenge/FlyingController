// Copyright Epic Games, Inc. All Rights Reserved.

#include "FlyingControllerEditorModule.h"

#include "SFlyingControllerMonitor.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "LevelEditor.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "FlyingControllerEditor"

void FFlyingControllerEditorModule::StartupModule()
{
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FFlyingControllerEditorModule::RegisterMenus));

	// 方便自动化验证与日常调试，也可在控制台手动敲。
	OpenMonitorCommand = MakeUnique<FAutoConsoleCommand>(
		TEXT("FlyingController.OpenMonitor"),
		TEXT("打开飞行手柄摇杆状态窗口"),
		FConsoleCommandDelegate::CreateRaw(this, &FFlyingControllerEditorModule::OpenMonitorWindow));

	// 启动早期（-ExecCmds 阶段）打开会被随后加载的编辑器布局关掉，
	// 所以自动化验证走这条延迟路径：-FlyingControllerAutoOpenMonitor。
	if (FParse::Param(FCommandLine::Get(), TEXT("FlyingControllerAutoOpenMonitor")))
	{
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FFlyingControllerEditorModule::AutoOpenMonitorTicker),
			20.0f);
	}
}

bool FFlyingControllerEditorModule::AutoOpenMonitorTicker(float DeltaTime)
{
	OpenMonitorWindow();
	return false;
}

void FFlyingControllerEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	if (MonitorWindow.IsValid())
	{
		MonitorWindow->RequestDestroyWindow();
		MonitorWindow.Reset();
	}
}

void FFlyingControllerEditorModule::RegisterMenus()
{
	// 用 LevelEditor 的工具条扩展器（经典做法）：按钮会插在 Content 按钮后面。
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<FExtensibilityManager> ExtensibilityManager = LevelEditorModule.GetToolBarExtensibilityManager();
	if (!ExtensibilityManager.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("FlyingController: 拿不到关卡编辑器工具条扩展器"));
		return;
	}

	TSharedRef<FExtender> Extender = MakeShared<FExtender>();
	Extender->AddToolBarExtension(
		TEXT("Content"),
		EExtensionHook::After,
		nullptr,
		FToolBarExtensionDelegate::CreateRaw(this, &FFlyingControllerEditorModule::AddToolbarButton));

	ExtensibilityManager->AddExtender(Extender);

	UE_LOG(LogTemp, Log, TEXT("FlyingController: 工具栏按钮已挂到 LevelEditor 主工具栏（Content 之后）"));
}

void FFlyingControllerEditorModule::AddToolbarButton(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateRaw(this, &FFlyingControllerEditorModule::OpenMonitorWindow)),
		TEXT("FlyingController.OpenMonitor"),
		LOCTEXT("ButtonLabel", "手柄"),
		LOCTEXT("ButtonTooltip", "打开飞行手柄摇杆状态面板"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FFlyingControllerEditorModule::OpenMonitorWindow()
{
	if (MonitorWindow.IsValid())
	{
		MonitorWindow->BringToFront(/*bForce=*/true);
		return;
	}

	MonitorWindow = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "飞行手柄"))
		.ClientSize(FVector2D(1400.0f, 900.0f))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		[
			SNew(SFlyingControllerMonitor)
		];

	FSlateApplication::Get().AddWindow(MonitorWindow.ToSharedRef());

	MonitorWindow->SetOnWindowClosed(FOnWindowClosed::CreateLambda(
		[this](const TSharedRef<SWindow>& ClosedWindow)
		{
			MonitorWindow.Reset();
		}));

	UE_LOG(LogTemp, Log, TEXT("FlyingController: 打开摇杆状态窗口"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FFlyingControllerEditorModule, FlyingControllerEditor)
