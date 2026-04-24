// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"

// Forward declare to keep POD headers free of reflection plumbing.
class UFunction;
class UClass;

/**
 * One parameter of a tool. Values mirror the JSON schema format we emit to
 * LLMs (see TOOL_REGISTRY.md §1.3 for the type mapping).
 *
 * TypeJson is a pre-serialized JSON-schema fragment (e.g. `{"type":"string"}`
 * or `{"type":"array","items":{"type":"string"}}`). Storing it as a string
 * keeps downstream consumers zero-alloc when emitting the full tool JSON.
 */
struct FVesselParameterSchema
{
	/** Parameter identifier as declared in C++. LLM sees the snake_case'd form. */
	FName Name;

	/** Pre-rendered JSON schema fragment describing the value's shape. */
	FString TypeJson;

	bool bRequired = true;
	bool bIsReturnValue = false;

	/** Optional — pulled from UPARAM/meta ToolDescription, or the doxygen @param tag. */
	FString Description;
};

/**
 * Full schema describing a single tool, as extracted by
 * FVesselReflectionScanner from a UFUNCTION meta block.
 *
 * Invariant: once constructed and inserted into FVesselToolRegistry, a schema
 * is immutable for its lifetime. Mutation only happens via full rescan.
 */
struct FVesselToolSchema
{
	// -------- Identity --------
	/** Tool name as surfaced to the LLM (== UFunction name). */
	FName Name;

	/** e.g. "DataTable" / "Asset" / "Blueprint" / "Meta" / "Code" / "Validator". */
	FString Category;

	/** Free-form natural language description — goes into the LLM prompt. */
	FString Description;

	// -------- Policy --------
	bool bRequiresApproval = true;
	bool bIrreversibleHint = false;
	bool bBatchEligible = false;

	/** Vessel semver string; tools with a higher MinVesselVersion are skipped on older cores. */
	FString MinVesselVersion;

	TArray<FString> Tags;

	// -------- Parameters --------
	TArray<FVesselParameterSchema> Parameters;

	/** Pre-rendered JSON schema for the return value. Empty → void return. */
	FString ReturnTypeJson;

	// -------- Source (for diagnostics / audit) --------
	FString SourceClassName;
	FString SourceFunctionName;
	FString SourceModuleName;

	/**
	 * Raw pointer to the reflected UFunction. UClass / UFunction live for the
	 * whole process lifetime (they are not GC'd), so storing raw is safe. Do
	 * NOT call UFunction::Invoke directly from schema consumers — go through
	 * FVesselToolRegistry which applies arg validation and policy checks.
	 */
	UFunction* Function = nullptr;
};
