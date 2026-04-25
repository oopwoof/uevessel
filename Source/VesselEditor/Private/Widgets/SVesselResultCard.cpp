// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Widgets/SVesselResultCard.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "VesselResultCard"

void SVesselResultCard::Construct(const FArguments& InArgs)
{
	const int32 StepIndex     = InArgs._StepIndex;
	const FName ToolName      = InArgs._ToolName;
	const bool  bWasError     = InArgs._bWasError;
	const FString& ResultJson = InArgs._ResultJson;
	const FString& ErrorMsg   = InArgs._ErrorMessage;

	const FText Header = FText::Format(
		bWasError
			? LOCTEXT("HeaderError",  "Step {0} · {1} · error")
			: LOCTEXT("HeaderOk",     "Step {0} · {1} · ok"),
		FText::AsNumber(StepIndex),
		FText::FromName(ToolName));

	const FLinearColor HeaderColor = bWasError
		? FLinearColor(0.85f, 0.40f, 0.40f, 1.0f)
		: FLinearColor(0.55f, 0.85f, 0.55f, 1.0f);

	const FString BodyText = bWasError
		? (ErrorMsg.IsEmpty() ? FString(TEXT("(no error message)")) : ErrorMsg)
		: (ResultJson.IsEmpty() ? FString(TEXT("(empty)")) : ResultJson);

	ChildSlot
	[
		SNew(SBorder)
		.Padding(FMargin(6, 4))
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SExpandableArea)
			.InitiallyCollapsed(true)
			.HeaderContent()
			[
				SNew(STextBlock)
				.Text(Header)
				.ColorAndOpacity(HeaderColor)
			]
			.BodyContent()
			[
				SNew(SBox)
				.MaxDesiredHeight(180.0f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SNew(STextBlock)
						.Text(FText::FromString(BodyText))
						.AutoWrapText(true)
					]
				]
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
