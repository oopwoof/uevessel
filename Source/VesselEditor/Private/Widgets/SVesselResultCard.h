// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Snapshot card for one tool execution result. Appended to the chat scroll
 * the moment FVesselSessionMachine::OnStepExecuted fires. Read-only.
 *
 * Visual: collapsible header showing step index, tool name, and ok/error
 * badge; body shows result_json (mono, scrollable) or error message.
 * Default-collapsed because most users only care about the verdict.
 */
class SVesselResultCard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVesselResultCard)
		: _StepIndex(0), _bWasError(false) {}
		SLATE_ARGUMENT(int32, StepIndex)
		SLATE_ARGUMENT(FName, ToolName)
		SLATE_ARGUMENT(FString, ResultJson)
		SLATE_ARGUMENT(bool, bWasError)
		SLATE_ARGUMENT(FString, ErrorMessage)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};
