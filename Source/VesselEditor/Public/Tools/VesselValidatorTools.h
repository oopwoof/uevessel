// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VesselValidatorTools.generated.h"

/**
 * Asset validator tool — drives UEditorValidatorSubsystem, collects
 * errors/warnings, returns a structured JSON report. Editor-only by nature
 * (UEditorValidatorSubsystem lives in the DataValidation module).
 *
 * See docs/engineering/TOOL_REGISTRY.md §5 for error code semantics.
 */
UCLASS()
class UVesselValidatorTools : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Run registered validators against a single asset. Returns JSON with
	 * fields: ok, asset, result ("valid" | "invalid" | "not_validated"),
	 * errors (array of strings), warnings (array of strings).
	 */
	UFUNCTION(BlueprintCallable, meta=(
		AgentTool="true",
		ToolCategory="Validator",
		RequiresApproval="false",
		ToolDescription="Run UEditorValidator validators on an asset. Returns JSON with result, errors, warnings arrays."))
	static FString RunAssetValidator(const FString& AssetPath);
};
