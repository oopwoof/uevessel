// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Registry/VesselToolSchema.h"

class UClass;
class UFunction;
class FProperty;

/**
 * Static utility that walks the UE reflection system and emits tool schemas
 * for every UFUNCTION tagged with `meta=(AgentTool="true")`.
 *
 * Scanning depends on UE Editor-only metadata (`WITH_EDITOR`). Packaged
 * runtime builds strip function metadata, so BuildToolSchemas returns an
 * empty array there — intentional. Vessel v0.1 is an editor-time tool.
 *
 * See docs/engineering/TOOL_REGISTRY.md §2.
 */
class VESSELCORE_API FVesselReflectionScanner
{
public:
	/** Walk TObjectIterator<UClass>, build one FVesselToolSchema per eligible UFunction. */
	static TArray<FVesselToolSchema> BuildToolSchemas();

	/** Build schema for a specific function (used by tests with known fixtures). */
	static FVesselToolSchema BuildSchemaForFunction(UClass* OwningClass, UFunction* Function);

	/** Map a single FProperty to a JSON-schema fragment (string/int/array/object/...). */
	static FString PropertyToJsonSchema(FProperty* Prop);
};
