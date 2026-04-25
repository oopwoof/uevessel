// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"

/**
 * Per-million-token USD pricing for known LLM models. Returned by lookup;
 * EstimateCostUsd combines a model's per-MTok rates with token counts to
 * produce a per-call USD estimate.
 *
 * Estimates only — Anthropic's billed amount is authoritative. The numbers
 * here track Anthropic's public pricing page as of v0.2 (Apr 2026); update
 * the table in VesselLlmPricing.cpp when Anthropic re-prices.
 */
struct FVesselLlmModelPrice
{
	/** USD per 1M input tokens. */
	float InputUsdPerMTok = 0.0f;
	/** USD per 1M output tokens. */
	float OutputUsdPerMTok = 0.0f;
	/** USD per 1M cache-read tokens (typically 10% of input rate for Anthropic). */
	float CacheReadUsdPerMTok = 0.0f;
	/** USD per 1M cache-creation tokens (typically 125% of input rate for Anthropic). */
	float CacheCreationUsdPerMTok = 0.0f;
};

/**
 * Pricing lookup. Matching is case-insensitive prefix — "claude-sonnet-4-6"
 * matches "claude-sonnet-4-6-20260101" etc. Returns a zero-priced entry
 * (so cost stays $0) for unknown models rather than failing — agents using
 * private / new models won't crash on cost accumulation.
 */
class VESSELCORE_API FVesselLlmPricing
{
public:
	static FVesselLlmModelPrice Lookup(const FString& ModelId);

	/** Estimate one call's USD cost from a model id and token counts. */
	static float EstimateCostUsd(
		const FString& ModelId,
		int32 InputTokens,
		int32 OutputTokens,
		int32 CacheReadTokens = 0,
		int32 CacheCreationTokens = 0);
};
