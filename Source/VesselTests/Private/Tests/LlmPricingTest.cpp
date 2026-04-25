// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Misc/AutomationTest.h"
#include "Llm/VesselLlmPricing.h"

/**
 * Pricing-table sanity checks. The exact USD numbers track Anthropic's
 * public pricing snapshot at v0.2 alpha; these tests assert (a) the table
 * is non-empty for the three models Vessel ships with as defaults, (b)
 * lookup is case-insensitive prefix-match (so dated suffixes still work),
 * and (c) unknown models cleanly return zero rather than exploding.
 *
 * These tests must NOT assert specific USD numbers — pricing changes
 * regularly and that would create churn. We assert relative ordering
 * (Opus > Sonnet > Haiku) which is structurally stable.
 */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselLlmPricingKnownModels,
	"Vessel.Llm.Pricing.KnownModels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselLlmPricingKnownModels::RunTest(const FString& /*Parameters*/)
{
	const FVesselLlmModelPrice Sonnet =
		FVesselLlmPricing::Lookup(TEXT("claude-sonnet-4-6"));
	const FVesselLlmModelPrice Haiku =
		FVesselLlmPricing::Lookup(TEXT("claude-haiku-4-5"));
	const FVesselLlmModelPrice Opus =
		FVesselLlmPricing::Lookup(TEXT("claude-opus-4-7"));

	TestTrue(TEXT("Sonnet input rate > 0"),  Sonnet.InputUsdPerMTok  > 0.0f);
	TestTrue(TEXT("Sonnet output rate > 0"), Sonnet.OutputUsdPerMTok > 0.0f);
	TestTrue(TEXT("Haiku input rate > 0"),   Haiku.InputUsdPerMTok   > 0.0f);
	TestTrue(TEXT("Opus input rate > 0"),    Opus.InputUsdPerMTok    > 0.0f);

	// Relative ordering (Anthropic has held this for years).
	TestTrue(TEXT("Opus input > Sonnet input"),
		Opus.InputUsdPerMTok > Sonnet.InputUsdPerMTok);
	TestTrue(TEXT("Sonnet input > Haiku input"),
		Sonnet.InputUsdPerMTok > Haiku.InputUsdPerMTok);

	// Output is always more expensive than input.
	TestTrue(TEXT("Sonnet output > input"),
		Sonnet.OutputUsdPerMTok > Sonnet.InputUsdPerMTok);

	// Cache read rate is the cheapest (it's a discount tier).
	TestTrue(TEXT("Sonnet cache-read < input"),
		Sonnet.CacheReadUsdPerMTok < Sonnet.InputUsdPerMTok);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselLlmPricingDatedSuffix,
	"Vessel.Llm.Pricing.PrefixMatchDatedSuffix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselLlmPricingDatedSuffix::RunTest(const FString& /*Parameters*/)
{
	// Anthropic appends a date stamp on served model ids, e.g.
	// "claude-sonnet-4-6-20260115". Lookup must still match.
	const FVesselLlmModelPrice ServedSnapshot =
		FVesselLlmPricing::Lookup(TEXT("claude-sonnet-4-6-20260115"));
	TestTrue(TEXT("Dated Sonnet still matched by prefix"),
		ServedSnapshot.InputUsdPerMTok > 0.0f);

	const FVesselLlmModelPrice CaseShifted =
		FVesselLlmPricing::Lookup(TEXT("Claude-Sonnet-4-6"));
	TestTrue(TEXT("Lookup is case-insensitive"),
		CaseShifted.InputUsdPerMTok > 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselLlmPricingUnknownModel,
	"Vessel.Llm.Pricing.UnknownModelReturnsZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselLlmPricingUnknownModel::RunTest(const FString& /*Parameters*/)
{
	const FVesselLlmModelPrice Unknown =
		FVesselLlmPricing::Lookup(TEXT("acme-private-llm-7b"));
	TestEqual(TEXT("Unknown model input = 0"),  Unknown.InputUsdPerMTok,  0.0f);
	TestEqual(TEXT("Unknown model output = 0"), Unknown.OutputUsdPerMTok, 0.0f);

	const float Cost = FVesselLlmPricing::EstimateCostUsd(
		TEXT("acme-private-llm-7b"), 1000, 500);
	TestEqual(TEXT("Unknown model cost = 0"), Cost, 0.0f);

	const FVesselLlmModelPrice Empty = FVesselLlmPricing::Lookup(FString());
	TestEqual(TEXT("Empty id input = 0"), Empty.InputUsdPerMTok, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselLlmPricingEstimateMath,
	"Vessel.Llm.Pricing.EstimateMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselLlmPricingEstimateMath::RunTest(const FString& /*Parameters*/)
{
	// 1M input + 1M output for Sonnet should equal Sonnet.Input + Sonnet.Output.
	const FVesselLlmModelPrice Sonnet =
		FVesselLlmPricing::Lookup(TEXT("claude-sonnet-4-6"));
	const float Cost = FVesselLlmPricing::EstimateCostUsd(
		TEXT("claude-sonnet-4-6"),
		/*input*/ 1'000'000, /*output*/ 1'000'000);
	const float Expected = Sonnet.InputUsdPerMTok + Sonnet.OutputUsdPerMTok;
	TestEqual(TEXT("1M+1M tokens cost = input + output rate"), Cost, Expected);

	// Zero tokens = zero cost regardless of model.
	TestEqual(TEXT("Zero tokens = zero cost"),
		FVesselLlmPricing::EstimateCostUsd(TEXT("claude-sonnet-4-6"), 0, 0),
		0.0f);

	// Cache contributions add to the total.
	const float WithCache = FVesselLlmPricing::EstimateCostUsd(
		TEXT("claude-sonnet-4-6"),
		/*input*/ 0, /*output*/ 0,
		/*cache_read*/ 1'000'000, /*cache_create*/ 0);
	TestEqual(TEXT("1M cache-read tokens = cache-read rate"),
		WithCache, Sonnet.CacheReadUsdPerMTok);
	return true;
}
