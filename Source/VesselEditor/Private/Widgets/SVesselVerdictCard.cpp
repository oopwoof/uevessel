// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Widgets/SVesselVerdictCard.h"

#include "Session/VesselSessionTypes.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "VesselVerdictCard"

namespace VesselVerdictCardDetail
{
	static FLinearColor ColorFor(EVesselJudgeDecision Decision)
	{
		switch (Decision)
		{
			case EVesselJudgeDecision::Approve: return FLinearColor(0.30f, 0.75f, 0.40f, 1.0f);
			case EVesselJudgeDecision::Revise:  return FLinearColor(0.90f, 0.70f, 0.20f, 1.0f);
			case EVesselJudgeDecision::Reject:
			default:                            return FLinearColor(0.85f, 0.35f, 0.35f, 1.0f);
		}
	}
}

void SVesselVerdictCard::Construct(const FArguments& InArgs)
{
	const FVesselJudgeVerdict* Verdict = InArgs._Verdict;
	if (!Verdict)
	{
		ChildSlot[ SNullWidget::NullWidget ];
		return;
	}

	const FText Header = FText::Format(
		LOCTEXT("HeaderFmt", "Judge · {0}"),
		FText::FromString(JudgeDecisionToString(Verdict->Decision)));

	const FLinearColor Color = VesselVerdictCardDetail::ColorFor(Verdict->Decision);

	// Build reasoning body. Reasoning is always present; revise/reject add
	// the directive / reject reason on a second line.
	FString BodyText = Verdict->Reasoning;
	if (!Verdict->ReviseDirective.IsEmpty())
	{
		BodyText += TEXT("\n→ Revise directive: ");
		BodyText += Verdict->ReviseDirective;
	}
	if (!Verdict->RejectReason.IsEmpty())
	{
		BodyText += TEXT("\n→ Reject reason: ");
		BodyText += Verdict->RejectReason;
	}

	ChildSlot
	[
		SNew(SBorder)
		.Padding(FMargin(6, 4))
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(Header)
				.ColorAndOpacity(Color)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0, 2, 0, 0))
			[
				SNew(STextBlock)
				.Text(FText::FromString(BodyText))
				.AutoWrapText(true)
				.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f, 1.0f))
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
