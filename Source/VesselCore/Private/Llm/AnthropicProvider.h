// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Llm/ILlmProvider.h"

/**
 * Anthropic provider — real HTTP integration (v0.1 scaffold).
 *
 * The full tool-calling + streaming contract lands in Step 4+. This file
 * gives the shape:
 *   - Reads endpoint / model / headers from UVesselProjectSettings
 *   - Reads API key via FVesselAuth::GetAnthropicApiKey() (env > user settings)
 *   - Enforces HTTPS via FVesselAuth::IsEndpointPermitted before sending
 *   - Returns FLlmResponse with ConfigError if settings are incomplete
 *
 * Registered in FVesselCoreModule::StartupModule alongside FVesselMockProvider.
 */
class FAnthropicProvider : public ILlmProvider
{
public:
	virtual FString GetProviderId() const override { return TEXT("anthropic"); }
	virtual bool SupportsToolCalling() const override { return true; }
	virtual TFuture<FLlmResponse> SendAsync(const FLlmRequest& Request) override;

private:
	static FString ResolveEndpoint(const FString& Override);
	static FString BuildRequestBodyJson(const FLlmRequest& Request);
	static FLlmResponse ParseResponse(int32 HttpCode, const FString& Body);
};
