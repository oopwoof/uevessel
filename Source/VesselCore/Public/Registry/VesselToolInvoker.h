// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Registry/VesselResult.h"

class FJsonObject;

/**
 * Invokes a registered tool by name + JSON args. This is the thin pipeline
 * between "LLM produced a tool call" and "UFunction::ProcessEvent fires":
 *
 *   1. Lookup schema in FVesselToolRegistry
 *   2. Parse ArgsJson (via FVesselJsonSanitizer — tolerates markdown fences)
 *   3. Marshal each JSON value into a reflection-allocated param buffer
 *   4. Open a FVesselTransactionScope if policy demands
 *   5. CDO->ProcessEvent(Function, Params)
 *   6. Serialize return value as JSON string
 *   7. Release param buffer
 *
 * Step 3b scope: supports FString, FName, int32/int64, bool,
 * TArray<FString>, TArray<FName>. TMap / USTRUCT / UEnum marshaling in 3c.
 *
 * HITL is NOT wired here — invoking this directly bypasses approval. That
 * is deliberate: Session Machine (step 4) is the one that consults the
 * HITL Gate before calling Invoke. Invoker is the pure plumbing.
 *
 * Thread affinity: game thread only (UObject::ProcessEvent requirement).
 */
class VESSELCORE_API FVesselToolInvoker
{
public:
	struct FInvokeOptions
	{
		/** For session log correlation; appears in the transaction description. */
		FString SessionId;
	};

	/**
	 * Invoke the named tool. Return value wraps the function's serialized
	 * return as JSON string. Errors carry structured EVesselResultCode.
	 */
	static FVesselResult<FString> Invoke(
		FName ToolName,
		const FString& ArgsJson,
		const FInvokeOptions& Options = {});
};
