// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VesselTestToolFixture.generated.h"

/**
 * Row struct used by DataTable tool tests. Small, stable, covers string/int/bool.
 */
USTRUCT()
struct FVesselTestRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Title;

	UPROPERTY()
	int32 Age = 0;

	UPROPERTY()
	bool bActive = false;
};

/**
 * Static fixture class with AgentTool-marked UFUNCTIONs used by automation
 * tests. Lives in VesselTests so the real reflection scan observes it.
 *
 * Keep the surface minimal — add new fixture functions only when required by
 * a new test case. Fixtures accidentally broadening scan results cause
 * fragile cross-test coupling.
 */
UCLASS()
class UVesselTestToolFixture : public UObject
{
	GENERATED_BODY()

public:
	/** Minimal shape — one string + one array-of-string + one int. Used to validate parameter schema emission. */
	UFUNCTION(BlueprintCallable, meta=(
		AgentTool="true",
		ToolCategory="Test",
		RequiresApproval="false",
		ToolDescription="Fixture tool for Vessel automation tests. Does nothing."))
	static FString FixtureRead(const FString& Path, const TArray<FString>& Keys, int32 Limit);

	/** Tagged irreversible + batch-eligible, to verify the scanner surfaces those policy flags. */
	UFUNCTION(BlueprintCallable, meta=(
		AgentTool="true",
		ToolCategory="Test",
		RequiresApproval="true",
		IrreversibleHint="true",
		BatchEligible="true",
		ToolTags="fixture,irreversible",
		ToolDescription="Fixture tool that pretends to mutate external state."))
	static bool FixtureIrreversibleWrite(const FString& Target);

	/** A function WITHOUT the AgentTool meta — must not appear in scan results. */
	UFUNCTION(BlueprintCallable)
	static int32 NotAnAgentTool(int32 X);
};
