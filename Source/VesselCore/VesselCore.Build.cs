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
			"AssetRegistry",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});

		// Transaction scope (FScopedTransaction) lives in UnrealEd. VesselCore
		// stays Runtime-compatible by conditionally linking UnrealEd only when
		// the target builds the editor — the transaction scope is guarded with
		// WITH_EDITOR at the source level, so non-editor builds compile cleanly.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		// Vessel does not use exceptions; errors flow through FVesselResult<T>.
		// See docs/engineering/CODING_STYLE.md §7.
		bEnableExceptions = false;
	}
}
