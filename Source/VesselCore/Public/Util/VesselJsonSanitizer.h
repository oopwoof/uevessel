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
 *   - Do NOT attempt to "fix" malformed JSON beyond fence stripping +
 *     first-balanced-{...} extraction. If both fail, return false with an
 *     actionable LLM-readable error message via the caller.
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

private:
	/** Remove leading ```json / ```\n and trailing ``` fences if present. */
	static FString StripMarkdownFences(const FString& Raw);

	/** Walk from first '{', honoring string quoting, until balanced '}'. -1 on failure. */
	static int32 FindBalancedObjectEnd(const FString& Text, int32 StartIdx);
};
