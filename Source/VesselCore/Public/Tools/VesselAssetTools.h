// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VesselAssetTools.generated.h"

/**
 * Read-only asset discovery tools. Backed by IAssetRegistry, which is a
 * Runtime module — these tools compile fine outside the editor but only
 * produce meaningful data when the asset registry has indexed content
 * (always true in Editor, true in packaged games after AssetRegistry loads).
 *
 * See docs/engineering/TOOL_REGISTRY.md for category conventions.
 */
UCLASS()
class VESSELCORE_API UVesselAssetTools : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * List assets under a given content path. Returns JSON array of asset
	 * soft-path strings. `ContentPath` examples: "/Game" or "/Game/Characters".
	 */
	UFUNCTION(BlueprintCallable, meta=(
		AgentTool="true",
		ToolCategory="Meta",
		RequiresApproval="false",
		ToolDescription="List assets under a content path. Returns JSON array of asset soft-path strings."))
	static FString ListAssets(const FString& ContentPath, bool bRecursive);

	/**
	 * Read high-level metadata for a single asset. Returns a JSON object with
	 * class name, outer package, tag values, and basic flags. Does NOT load
	 * the asset body — uses FAssetData directly so it is cheap to call.
	 */
	UFUNCTION(BlueprintCallable, meta=(
		AgentTool="true",
		ToolCategory="Meta",
		RequiresApproval="false",
		ToolDescription="Read metadata for an asset. Returns JSON with class, package, tags. Does not load asset body."))
	static FString ReadAssetMetadata(const FString& AssetPath);
};
