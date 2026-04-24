// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

// ---------------------------------------------------------------------------
// Log categories — see docs/engineering/CODING_STYLE.md §3.2.
// Filter at runtime: -LogCmds="LogVesselSession Verbose"
// ---------------------------------------------------------------------------

VESSELCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogVessel, Log, All);
VESSELCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogVesselRegistry, Log, All);
VESSELCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogVesselSession, Log, All);
VESSELCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogVesselHITL, Log, All);
VESSELCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogVesselLlm, Log, All);
VESSELCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogVesselCost, Log, All);

/**
 * Short-hand for the default LogVessel category.
 * For specialized categories, call UE_LOG(LogVesselRegistry, ...) directly.
 * Never use UE_LOG(LogTemp, ...) — see CODING_STYLE.md §3.1.
 */
#define VESSEL_LOG(Verbosity, Format, ...) \
	UE_LOG(LogVessel, Verbosity, Format, ##__VA_ARGS__)
