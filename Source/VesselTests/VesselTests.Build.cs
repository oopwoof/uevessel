// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

using UnrealBuildTool;

public class VesselTests : ModuleRules
{
	public VesselTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"VesselCore",
			"VesselEditor",  // for ValidatorTools tests
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AutomationController",
			"AssetRegistry",
			// Tests directly construct FJsonObject and call SetStringField /
			// TryGetStringField; UE 5.7 requires explicit Json deps even when
			// VesselCore re-exports them.
			"Json",
			"JsonUtilities",
		});

		bEnableExceptions = false;
	}
}
