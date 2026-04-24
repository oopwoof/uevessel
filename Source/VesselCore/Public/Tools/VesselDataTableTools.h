// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VesselDataTableTools.generated.h"

class UDataTable;

/**
 * DataTable-focused tools — the first concrete built-in tool family.
 *
 * All tools here are static UFUNCTIONs with AgentTool meta; they get
 * auto-discovered by FVesselReflectionScanner on ScanAll.
 *
 * For testability, the actual row-walking logic is factored into
 * `ReadRowsJson(UDataTable*, ...)` so unit tests can exercise it on an
 * in-memory DataTable without going through asset path resolution.
 */
UCLASS()
class VESSELCORE_API UVesselDataTableTools : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Read rows from a DataTable asset. Returns a JSON object keyed by
	 * row name; each value is a JSON object of the row's UPROPERTY fields.
	 * When RowNames is empty, returns all rows.
	 */
	UFUNCTION(BlueprintCallable, meta=(
		AgentTool="true",
		ToolCategory="DataTable",
		RequiresApproval="false",
		ToolDescription="Read rows from a DataTable asset. Returns a JSON object keyed by row name; empty RowNames returns all rows."))
	static FString ReadDataTable(const FString& AssetPath, const TArray<FName>& RowNames);

	/** Test-visible helper: same logic as ReadDataTable, but against an in-memory DataTable. */
	static FString ReadRowsJson(UDataTable* Table, const TArray<FName>& RowNames);
};
