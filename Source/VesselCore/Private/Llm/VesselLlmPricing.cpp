// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Llm/VesselLlmPricing.h"

namespace VesselLlmPricingTable
{
	struct FEntry
	{
		const TCHAR* Prefix;
		FVesselLlmModelPrice Price;
	};

	/**
	 * Anthropic public pricing as of v0.2 alpha (April 2026).
	 *
	 * Source: https://www.anthropic.com/pricing  (cross-checked with
	 * Anthropic Console billing dashboard).
	 *
	 * Cache tiers follow Anthropic's standard policy:
	 *   • cache read   = 10% of input rate
	 *   • cache create = 125% of input rate (5-minute TTL)
	 *
	 * Update this table whenever Anthropic re-prices. Until then, every
	 * cost number Vessel surfaces is an estimate against this snapshot.
	 */
	static const FEntry kTable[] =
	{
		// Opus 4.7 — top tier
		{ TEXT("claude-opus-4-7"),    { 15.0f, 75.0f, 1.5f,  18.75f } },

		// Sonnet 4.6 — daily-driver default
		{ TEXT("claude-sonnet-4-6"),  {  3.0f, 15.0f, 0.30f,  3.75f } },

		// Haiku 4.5 — Judge default + cheap batch
		{ TEXT("claude-haiku-4-5"),   {  1.0f,  5.0f, 0.10f,  1.25f } },

		// Older 4.x lines kept around for legacy projects.
		{ TEXT("claude-opus-4-5"),    { 15.0f, 75.0f, 1.5f,  18.75f } },
		{ TEXT("claude-sonnet-4-5"),  {  3.0f, 15.0f, 0.30f,  3.75f } },
	};
}

FVesselLlmModelPrice FVesselLlmPricing::Lookup(const FString& ModelId)
{
	if (ModelId.IsEmpty())
	{
		return FVesselLlmModelPrice{};
	}
	for (const VesselLlmPricingTable::FEntry& E : VesselLlmPricingTable::kTable)
	{
		if (ModelId.StartsWith(E.Prefix, ESearchCase::IgnoreCase))
		{
			return E.Price;
		}
	}
	return FVesselLlmModelPrice{};
}

float FVesselLlmPricing::EstimateCostUsd(
	const FString& ModelId,
	int32 InputTokens, int32 OutputTokens,
	int32 CacheReadTokens, int32 CacheCreationTokens)
{
	const FVesselLlmModelPrice P = Lookup(ModelId);
	const float kPerMTok = 1.0f / 1'000'000.0f;
	return
		P.InputUsdPerMTok         * static_cast<float>(InputTokens)         * kPerMTok +
		P.OutputUsdPerMTok        * static_cast<float>(OutputTokens)        * kPerMTok +
		P.CacheReadUsdPerMTok     * static_cast<float>(CacheReadTokens)     * kPerMTok +
		P.CacheCreationUsdPerMTok * static_cast<float>(CacheCreationTokens) * kPerMTok;
}
