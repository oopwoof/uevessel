// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"

/**
 * Stable tab identifiers used by the Slate dock system. Centralized so both
 * the spawner registration (in VesselEditor::StartupModule) and any code
 * that invokes the tab (menu entries, console commands) agree on the name.
 */
namespace VesselTabIds
{
	/** Main chat + diff + approval panel — the hero surface. */
	static const FName ChatPanel = TEXT("VesselChatPanel");
}
