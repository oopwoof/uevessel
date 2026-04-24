// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "Llm/VesselLlmTypes.h"

/**
 * Abstract LLM provider. Implementations live in Private/Llm/*.
 * Contract:
 *   - SendAsync MUST NOT block the game thread.
 *   - SendAsync MUST return a completed future within the Request's timeout
 *     (enforced by the Session Machine, but providers should not add their own
 *     longer timeouts).
 *   - On any failure, return FLlmResponse::MakeError with an actionable message.
 *     Do not throw; Vessel disables exceptions (see CODING_STYLE §7.2).
 *
 * Provider identifiers must be stable lower-case strings:
 *   "mock", "anthropic", "azure-openai", "custom", ...
 */
class VESSELCORE_API ILlmProvider
{
public:
	virtual ~ILlmProvider() = default;

	virtual FString GetProviderId() const = 0;
	virtual bool SupportsToolCalling() const { return true; }

	virtual TFuture<FLlmResponse> SendAsync(const FLlmRequest& Request) = 0;
};
