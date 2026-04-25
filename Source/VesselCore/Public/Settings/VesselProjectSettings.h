// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VesselProjectSettings.generated.h"

/**
 * Team-shared Vessel settings persisted to Config/DefaultVessel.ini.
 *
 * Only non-sensitive values live here (Endpoint URL, Model name, etc).
 * SECRETS (API keys, Bearer tokens) belong in UVesselUserSettings instead —
 * that class is EditorPerProjectUserSettings so it stays under Saved/ and
 * never enters version control.
 *
 * See docs/engineering/BUILD.md §4 and docs/engineering/ARCHITECTURE.md ADR-006.
 */
UCLASS(config=Vessel, defaultconfig, meta=(DisplayName="Vessel (Project)"))
class VESSELCORE_API UVesselProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UVesselProjectSettings();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** Logical provider id; currently "anthropic" | "azure-openai" | "custom". Mock handled separately. */
	UPROPERTY(EditAnywhere, config, Category="LLM", meta=(DisplayName="Provider"))
	FString Provider;

	/** Empty → use provider's public endpoint. Fill for Azure / enterprise gateway / proxy. */
	UPROPERTY(EditAnywhere, config, Category="LLM", meta=(DisplayName="Endpoint (optional)"))
	FString Endpoint;

	/** Default model name for Planner. Judge model configured separately in agent templates. */
	UPROPERTY(EditAnywhere, config, Category="LLM", meta=(DisplayName="Default Model"))
	FString Model;

	/** Non-sensitive headers (e.g. routing hints). NEVER store Authorization / api-key here. */
	UPROPERTY(EditAnywhere, config, Category="LLM",
		meta=(DisplayName="Non-secret headers",
		      ToolTip="Team-safe headers only. Put Authorization / api-key in UVesselUserSettings."))
	TMap<FString, FString> NonSecretHeaders;

	/** Only honored when Endpoint points at http://localhost or http://127.0.0.1 (Core hard-coded check). */
	UPROPERTY(EditAnywhere, config, Category="Dev", meta=(DisplayName="Allow HTTP (localhost only)"))
	bool bAllowHttp;

	/**
	 * Which built-in agent template the chat panel boots with. Resolved via
	 * FVesselAgentTemplates::FindByName at session start. Empty falls back
	 * to "designer-assistant". Built-in names: designer-assistant,
	 * asset-pipeline, vessel-default. Custom templates (v0.3) plug in here.
	 */
	UPROPERTY(EditAnywhere, config, Category="Agents",
		meta=(DisplayName="Default Agent",
		      ToolTip="designer-assistant | asset-pipeline | vessel-default"))
	FString DefaultAgentName;

	/** Convenience accessor for the resolved settings singleton. */
	static const UVesselProjectSettings& GetRef();
};
