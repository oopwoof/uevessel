// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Llm/VesselLlmTypes.h"
#include "Registry/VesselToolSchema.h"
#include "Session/VesselSessionConfig.h"
#include "Session/VesselSessionTypes.h"

/**
 * Builds LLM prompts for Planner and Judge turns, and parses the expected
 * JSON response shapes back into FVesselPlan / FVesselJudgeVerdict.
 *
 * Two principles (see SESSION_MACHINE.md §3):
 *   1. Prompts are the ONLY place Vessel talks LLM-flavored English —
 *      the rest of the pipeline is structured data.
 *   2. Parsers are tolerant: we run FVesselJsonSanitizer before feeding
 *      responses to FJsonSerializer so markdown code fences don't kill us.
 */
class VESSELEDITOR_API FVesselPlannerPrompts
{
public:
	/**
	 * Construct a Planning request. Messages contain system + user input.
	 * AvailableTools are filtered per the agent template's allowed/denied
	 * lists before being embedded into the system prompt.
	 * @param ReviseDirective — when re-planning after a Revise verdict,
	 *        pass the judge's directive; otherwise leave empty.
	 */
	static FLlmRequest BuildPlanningRequest(
		const FVesselSessionConfig& Config,
		const FString& UserInput,
		const TArray<FVesselToolSchema>& AvailableTools,
		const FString& ReviseDirective = FString());

	/**
	 * Construct a Judge request evaluating a single executed step.
	 * @param ToolResultJson — the JSON string returned by the invoker
	 *        (already sanitized).
	 */
	static FLlmRequest BuildJudgeRequest(
		const FVesselSessionConfig& Config,
		const FVesselPlanStep& ExecutedStep,
		const FString& ToolResultJson);

	/**
	 * Parse a Planner LLM response into FVesselPlan. Always populates
	 * RawLlmResponse; sets bValid=false with ErrorMessage on failure.
	 */
	static FVesselPlan ParsePlanResponse(const FLlmResponse& Response);

	/**
	 * Parse a Judge LLM response into a FVesselJudgeVerdict.
	 * When parsing fails, returns a conservative Reject verdict with the
	 * raw parse error in Reasoning — "when in doubt, don't write".
	 */
	static FVesselJudgeVerdict ParseJudgeResponse(const FLlmResponse& Response);
};
