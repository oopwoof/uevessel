// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

struct FVesselJudgeVerdict;

/**
 * Snapshot card for one Judge verdict. Appended to the chat scroll the
 * moment FVesselSessionMachine::OnJudgeVerdict fires. Read-only.
 *
 * Visual: header colored by decision (Approve = green, Revise = amber,
 * Reject = red) + reasoning body. Always default-expanded because the
 * decision is the most important per-step signal for the user.
 */
class SVesselVerdictCard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVesselVerdictCard) {}
		SLATE_ARGUMENT(const FVesselJudgeVerdict*, Verdict)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};
