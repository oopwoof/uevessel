// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Widgets/SVesselPlanCard.h"

#include "Session/VesselSessionTypes.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "VesselPlanCard"

namespace VesselPlanCardDetail
{
	/** Plans with more than this many steps default-collapse to keep the chat tidy. */
	static constexpr int32 kAutoCollapseThreshold = 3;

	/** Truncate args JSON to keep the row a single line in the chat scroll. */
	static FString OneLine(const FString& In, int32 Max = 100)
	{
		FString Out = In.Replace(TEXT("\n"), TEXT(" "));
		Out.RemoveFromStart(TEXT(" "));
		if (Out.Len() > Max)
		{
			Out = Out.Left(Max - 1) + TEXT("…");
		}
		return Out;
	}
}

void SVesselPlanCard::Construct(const FArguments& InArgs)
{
	const FVesselPlan* Plan = InArgs._Plan;
	if (!Plan)
	{
		ChildSlot[ SNullWidget::NullWidget ];
		return;
	}

	const int32 N = Plan->Steps.Num();
	const FText HeaderText = FText::Format(
		LOCTEXT("PlanHeader", "Plan · {0} step(s)"), FText::AsNumber(N));

	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	for (const FVesselPlanStep& Step : Plan->Steps)
	{
		Body->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0, 2))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::Format(
						LOCTEXT("StepTitle", "step {0}: {1}"),
						FText::AsNumber(Step.StepIndex),
						FText::FromName(Step.ToolName)))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(12, 1, 0, 0))
				[
					SNew(STextBlock)
					.Text(FText::FromString(Step.Reasoning))
					.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f))
					.AutoWrapText(true)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(12, 1, 0, 0))
				[
					SNew(STextBlock)
					.Text(FText::FromString(
						FString(TEXT("args: "))
						+ VesselPlanCardDetail::OneLine(Step.ArgsJson)))
					.ColorAndOpacity(FLinearColor(0.55f, 0.65f, 0.85f, 1.0f))
				]
			];
	}

	const bool bDefaultExpand = N <= VesselPlanCardDetail::kAutoCollapseThreshold;

	ChildSlot
	[
		SNew(SBorder)
		.Padding(FMargin(6, 4))
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SExpandableArea)
			.InitiallyCollapsed(!bDefaultExpand)
			.HeaderContent()
			[
				SNew(STextBlock).Text(HeaderText)
			]
			.BodyContent()
			[
				Body
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
