// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VesselUserSettings.generated.h"

/**
 * Per-user Vessel secrets. Persisted to
 *   <Project>/Saved/Config/<Platform>/VesselUserSettings.ini
 * which is covered by the repo's .gitignore. NEVER commit this file.
 *
 * Surfaced under Editor Preferences → Plugins → Vessel (User · API Keys).
 * ProjectSettings-level UI intentionally does NOT expose these fields —
 * they would leak into DefaultVessel.ini otherwise.
 *
 * SECURITY INVARIANT: anything in this class is an opaque secret.
 *   - NEVER log these values
 *   - NEVER embed in session log JSONL
 *   - In Verbose LLM trace, render as "<redacted:N chars>"
 *
 * See docs/engineering/BUILD.md §4.0 / ARCHITECTURE.md ADR-006.
 */
UCLASS(config=EditorPerProjectUserSettings, meta=(DisplayName="Vessel (User · API Keys)"))
class VESSELCORE_API UVesselUserSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetContainerName() const override { return TEXT("Editor"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** Anthropic Console API key (https://console.anthropic.com/). Env var VESSEL_ANTHROPIC_API_KEY takes precedence. */
	UPROPERTY(EditAnywhere, config, Category="Secrets",
		meta=(DisplayName="Anthropic API Key", PasswordField=true))
	FString AnthropicApiKey;

	/** Full Authorization header value (e.g. "Bearer abc123"). Env VESSEL_GATEWAY_TOKEN wins. */
	UPROPERTY(EditAnywhere, config, Category="Secrets",
		meta=(DisplayName="Custom Gateway Authorization",
		      PasswordField=true,
		      ToolTip="Full header value including the 'Bearer ' prefix if required."))
	FString GatewayAuthorization;

	/** Azure OpenAI key for the 'api-key' header. Env VESSEL_AZURE_API_KEY wins. */
	UPROPERTY(EditAnywhere, config, Category="Secrets",
		meta=(DisplayName="Azure API Key", PasswordField=true))
	FString AzureApiKey;

	static const UVesselUserSettings& GetRef();
};
