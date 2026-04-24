// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

using UnrealBuildTool;

public class VesselCore : ModuleRules
{
	public VesselCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Runtime-safe module. Keep this list minimal; nothing Editor-only here.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"HTTP",
			"Json",
			"JsonUtilities",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});

		// Vessel does not use exceptions; errors flow through FVesselResult<T>.
		// See docs/engineering/CODING_STYLE.md §7.
		bEnableExceptions = false;
	}
}
