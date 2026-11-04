// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FlyingControllerEditor : ModuleRules
{
	public FlyingControllerEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"ToolMenus",
			"LevelEditor",
			"FlyingController"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
