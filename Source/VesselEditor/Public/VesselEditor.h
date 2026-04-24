// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
 * VesselEditor — Editor-only module hosting the Session Machine, Harness,
 * HITL Gate, Memory, and the Slate Dock Panel UI (SDockTab-based).
 *
 * Depends on VesselCore. Does not link back into VesselCore's clients.
 * See docs/engineering/ARCHITECTURE.md §3–§4.
 */
class FVesselEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
