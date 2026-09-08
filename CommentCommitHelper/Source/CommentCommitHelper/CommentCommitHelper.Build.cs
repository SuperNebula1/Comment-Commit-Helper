/*

Developer: Tanner A.

Date: 8/27/2026

*/

// Copyright 2026 Tanner A. All Rights Reserved.

using UnrealBuildTool;

public class CommentCommitHelper : ModuleRules
{
	public CommentCommitHelper(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore",
				"AssetRegistry",
				"DeveloperSettings",
				"EditorFramework",
				"InputCore",
				"Kismet",
				"Projects",
				"Settings",
				"Slate",
				"SlateCore",
				"SourceControl",
				"ToolMenus",
				"UnrealEd"
			}
		);
	}
}
