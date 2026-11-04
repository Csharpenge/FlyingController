// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FlyingController : ModuleRules
{
	public FlyingController(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ApplicationCore",		// IInputDeviceModule / IInputDevice
			"InputCore",			// FKey / EKeys
			"InputDevice",			// IInputDevice 与输入设备映射
			"DeveloperSettings"		// UDeveloperSettings
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			// 直接读 HID 报告：hid.dll 提供 HidP_/HidD_ 系列，setupapi 负责枚举设备接口。
			PublicSystemLibraries.AddRange(new string[]
			{
				"hid.lib",
				"setupapi.lib"
			});
		}
		else
		{
			throw new BuildException("FlyingController 目前只实现了 Windows 的 HID 读取（FS-SM600 走 USB HID）。");
		}
	}
}
