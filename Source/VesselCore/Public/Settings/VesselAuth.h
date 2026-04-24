// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"

/**
 * Thin static helper that resolves secrets following the documented precedence
 * (env vars > UVesselUserSettings > never-fallback-to-project-settings) and
 * enforces the localhost-only HTTP allow rule.
 *
 * Keep logic here minimal — Settings classes store, this class decides.
 */
class VESSELCORE_API FVesselAuth
{
public:
	/** Returns env VESSEL_ANTHROPIC_API_KEY if non-empty, else UVesselUserSettings.AnthropicApiKey. */
	static FString GetAnthropicApiKey();

	/** Returns env VESSEL_GATEWAY_TOKEN if non-empty, else UVesselUserSettings.GatewayAuthorization. */
	static FString GetGatewayAuthorization();

	/** Returns env VESSEL_AZURE_API_KEY if non-empty, else UVesselUserSettings.AzureApiKey. */
	static FString GetAzureApiKey();

	/**
	 * Hard-coded HTTP policy: allow http:// only when Endpoint starts with
	 *   "http://localhost" or "http://127.0.0.1"
	 * AND UVesselProjectSettings.bAllowHttp is true.
	 * Any other http:// URL returns false regardless of settings — this
	 * is the Core-level guard mentioned in ARCHITECTURE.md §2.4.
	 *
	 * https:// URLs always return true.
	 * Empty endpoint returns true (we rely on the provider's default HTTPS URL).
	 */
	static bool IsEndpointPermitted(const FString& Endpoint);

	/** Redacted representation of a secret, suitable for logs. "sk-ant-abc...": "<redacted:N>". */
	static FString Redact(const FString& Secret);

private:
	static FString GetEnvOrEmpty(const TCHAR* VarName);
};
