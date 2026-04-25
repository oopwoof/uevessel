// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

struct FVesselPlan;

/**
 * Snapshot card for one Planner output. Appended to the chat scroll the
 * moment FVesselSessionMachine::OnPlanReady fires. Read-only; not updated
 * after construction.
 *
 * Visual: collapsible header "Plan · N step(s)" + per-step rows showing
 * tool name, reasoning, and args JSON. Default-expanded for short plans
 * (<=3 steps), default-collapsed for longer to keep the chat scrollable.
 */
class SVesselPlanCard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVesselPlanCard) {}
		SLATE_ARGUMENT(const FVesselPlan*, Plan)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};
