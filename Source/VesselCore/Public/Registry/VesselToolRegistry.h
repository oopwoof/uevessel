// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Registry/VesselToolSchema.h"

/**
 * Process-global registry of discovered agent tools. Populated by
 * FVesselReflectionScanner::BuildToolSchemas and held until module shutdown.
 *
 * Access patterns:
 *   - Read-heavy: Planner, HITL UI, MCP server all query by name. Lock is RW.
 *   - Write-rare: ScanAll / ClearAll / InjectSchemaForTest only.
 *
 * See docs/engineering/TOOL_REGISTRY.md for the full contract.
 */
class VESSELCORE_API FVesselToolRegistry
{
public:
	static FVesselToolRegistry& Get();

	/** Replace all schemas by rescanning reflection. Safe to call repeatedly. */
	void ScanAll();

	/** Clear all schemas. Test-only in production code paths. */
	void ClearAll();

	/** Insert a hand-built schema. Used by tests; production code should rely on ScanAll. */
	void InjectSchemaForTest(const FVesselToolSchema& Schema);

	/** Number of registered tools. */
	int32 Num() const;

	/** Returns a pointer to the schema for the named tool, or nullptr if not found. */
	const FVesselToolSchema* FindSchema(FName ToolName) const;

	/** All registered tool names. Ordering is unspecified. */
	TArray<FName> ListToolNames() const;

	/** A snapshot copy of every schema. For JSON emission or introspection. */
	TArray<FVesselToolSchema> GetAllSchemas() const;

	/** Render the entire registry as a JSON array. */
	FString ToJsonString() const;

private:
	FVesselToolRegistry() = default;

	mutable FRWLock Lock;
	TMap<FName, FVesselToolSchema> Schemas;
};
