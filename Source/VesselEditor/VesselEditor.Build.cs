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
			// Direct use of FJsonObject / FJsonValue / FJsonObjectConverter.
			// UE 5.7 enforces that consumers list these explicitly; relying on
			// VesselCore's transitive export is no longer enough.
			"Json",
			"JsonUtilities",
		});

		bEnableExceptions = false;
	}
}
