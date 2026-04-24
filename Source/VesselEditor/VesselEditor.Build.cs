// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

using UnrealBuildTool;

public class VesselEditor : ModuleRules
{
	public VesselEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"VesselCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"Slate",
			"SlateCore",
			"EditorFramework",
			"EditorSubsystem",
			"ToolMenus",
			"InputCore",
			"WorkspaceMenuStructure",
			"DataValidation",   // UEditorValidatorSubsystem (UE 5.x)
			"AssetRegistry",
		});

		bEnableExceptions = false;
	}
}
