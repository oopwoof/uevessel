// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
 * VesselCore — runtime-safe module hosting the Tool Registry, Transaction Wrapper,
 * Validator Hooks, and LLM Adapter. Does not depend on any Editor-only module.
 *
 * See docs/engineering/ARCHITECTURE.md for the layered architecture.
 */
class FVesselCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
