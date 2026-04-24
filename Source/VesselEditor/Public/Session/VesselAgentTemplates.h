// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Session/VesselSessionConfig.h"

/**
 * Built-in agent templates shipped with v0.1. Each template captures role,
 * behavior hints, allowed tool scope, and Judge rubric for one workflow.
 *
 * v0.1 ships a single template — `designer-assistant` — that is sized for
 * the demo scenario in docs/product/USE_CASES.md §1 (batch NPC configuration).
 * v0.2 adds `asset-pipeline` and `dev-chat`.
 *
 * Templates are constructed as plain structs (no UObject / no config asset)
 * so they are trivially unit-testable and don't require an editor session
 * to exist at template-build time.
 */
class VESSELEDITOR_API FVesselAgentTemplates
{
public:
	/**
	 * Designer Assistant — the hero agent for v0.1. Scoped to DataTable +
	 * Meta categories; Judge rubric encourages staying inside existing schema
	 * and rejects attempts to silently extend row types.
	 */
	static FVesselAgentTemplate MakeDesignerAssistant();

	/** Lookup by name. Falls back to `MakeMinimalFallback()` when unknown. */
	static FVesselAgentTemplate FindByName(const FString& Name);

	/** All known template names, stable ordering. */
	static TArray<FString> ListNames();
};
