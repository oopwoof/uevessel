// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Strip LLM-style markdown fencing and best-effort extract the first JSON object.
 *
 * LLMs (Claude / Qwen / GPT) routinely wrap JSON in ```json ... ``` fences or
 * prepend explanatory prose. Raw UE FJsonSerializer rejects both. This helper
 * runs before parsing anywhere Vessel ingests LLM output.
 *
 * Policy:
 *   - Strip markdown fences and extract the first balanced {...} block.
 *   - If a strict parse of the extracted block fails, attempt ONE narrow
 *     repair pass: escape unescaped " inside string values where the LLM
 *     forgot to (e.g. "reasoning":"用户要的是\"典型数据\"" emitted as
 *     "reasoning":"用户要的是"典型数据""). Layer-A is a prompt rule asking
 *     the LLM to use 「」/'' or escape; this is belt-and-suspenders.
 *   - Do NOT embed prompt-engineering suggestions here. That is Planner's job.
 *
 * See TOOL_REGISTRY.md §5.5 and ARCHITECTURE.md §2.4.
 */
class VESSELCORE_API FVesselJsonSanitizer
{
public:
	/**
	 * Attempts to produce a clean JSON object string from Raw.
	 * Returns true and fills OutJson on success. Returns false on failure.
	 */
	static bool ExtractFirstJsonObject(const FString& Raw, FString& OutJson);

	/** Convenience: sanitize + parse into a TSharedPtr<FJsonObject>. */
	static bool ParseAsObject(const FString& Raw, TSharedPtr<FJsonObject>& OutObject);

	/**
	 * Repair pass for unescaped inner quotes. Scans the input as a JSON
	 * string, treats every `"` whose next non-whitespace char is NOT a
	 * JSON structural delimiter (`:`, `,`, `}`, `]`, or EOF) as an inner
	 * quote and escapes it with a leading backslash. Returns the repaired
	 * string. If no change was made, the result equals the input.
	 *
	 * Public so tests can pin the heuristic directly without going through
	 * Deserialize. Production callers should use ParseAsObject.
	 */
	static FString RepairUnescapedInnerQuotes(const FString& Text);

private:
	/** Remove leading ```json / ```\n and trailing ``` fences if present. */
	static FString StripMarkdownFences(const FString& Raw);

	/** Walk from first '{', honoring string quoting, until balanced '}'. -1 on failure. */
	static int32 FindBalancedObjectEnd(const FString& Text, int32 StartIdx);
};
