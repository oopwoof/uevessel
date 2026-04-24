// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Llm/ILlmProvider.h"

/**
 * Deterministic mock LLM provider for CI and unit tests.
 *
 * Usage:
 *   auto Mock = MakeShared<FVesselMockProvider>();
 *   Mock->SetFixtureForLastUserMessage("expected prompt", Response);
 *   FLlmProviderRegistry::Get().InjectMock(Mock);
 *
 * Matching strategy: by the content of the last user-role message. This is
 * deliberately simple — tests should be explicit about their expected input.
 * Unmatched requests fall back to DefaultResponse.
 *
 * See ARCHITECTURE.md §2.4 — Mock provider is a v0.1 must-ship requirement,
 * not an optional testing nicety.
 */
class VESSELCORE_API FVesselMockProvider : public ILlmProvider
{
public:
	FVesselMockProvider();

	virtual FString GetProviderId() const override { return TEXT("mock"); }
	virtual bool SupportsToolCalling() const override { return true; }
	virtual TFuture<FLlmResponse> SendAsync(const FLlmRequest& Request) override;

	/** Map the given last-user-message substring to a canned response. */
	void SetFixtureForLastUserMessage(const FString& UserMessageExact, const FLlmResponse& Response);

	/** Response returned when no fixture matches. Defaults to bOk=false ConfigError. */
	void SetDefaultResponse(const FLlmResponse& Response);

	void ClearFixtures();

	/** For assertions: how many SendAsync calls have we received. */
	int32 GetCallCount() const;

private:
	mutable FCriticalSection Lock;
	TMap<FString, FLlmResponse> FixturesByLastUser;
	FLlmResponse DefaultResponse;
	int32 CallCount = 0;

	static FString ExtractLastUserContent(const FLlmRequest& Request);
};
