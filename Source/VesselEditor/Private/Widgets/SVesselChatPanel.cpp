// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Widgets/SVesselChatPanel.h"

#include "VesselLog.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "VesselChatPanel"

void SVesselChatPanel::Construct(const FArguments& /*InArgs*/)
{
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Docking.Tab.ContentAreaBrush"))
		.Padding(FMargin(8.f))
		[
			SNew(SVerticalBox)

			// -------- Header row: agent, status, cost --------
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 6.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("VesselHeaderLabel", "🔷 Vessel"))
					.TextStyle(FAppStyle::Get(), "HeaderText")
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(12.f, 0.f)
				[
					SAssignNew(AgentStatus, STextBlock)
					.Text(LOCTEXT("VesselIdleStatus", "Agent: idle"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(CostLabel, STextBlock)
					.Text(LOCTEXT("VesselCostPlaceholder", "$0.00"))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 6.f)
			[
				SNew(SSeparator)
			]

			// -------- Main split: chat history | diff preview --------
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)

				// Chat
				+ SSplitter::Slot()
				.Value(0.6f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.FillHeight(1.f)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.Padding(4.f)
						[
							SAssignNew(ChatScroll, SScrollBox)
							.ScrollBarAlwaysVisible(false)
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 4.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						[
							SAssignNew(InputBox, SEditableTextBox)
							.HintText(LOCTEXT("VesselInputHint",
								"Describe what you want the agent to do..."))
							.OnTextCommitted_Lambda([this](const FText& /*T*/, ETextCommit::Type Commit)
							{
								if (Commit == ETextCommit::OnEnter)
								{
									HandleSendClicked();
								}
							})
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(4.f, 0.f, 0.f, 0.f)
						[
							SNew(SButton)
							.Text(LOCTEXT("VesselSendButton", "Send"))
							.OnClicked(FOnClicked::CreateSP(this, &SVesselChatPanel::HandleSendClicked))
						]
					]
				]

				// Diff preview
				+ SSplitter::Slot()
				.Value(0.4f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(4.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f, 0.f, 0.f, 4.f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("VesselDiffLabel", "Diff preview / Agent reasoning"))
							.TextStyle(FAppStyle::Get(), "SmallText")
						]
						+ SVerticalBox::Slot()
						.FillHeight(1.f)
						[
							SAssignNew(DiffArea, SMultiLineEditableTextBox)
							.IsReadOnly(true)
							.Text(LOCTEXT("VesselDiffEmpty",
								"(No pending change. Vessel will show the proposed diff + validator results here when a tool wants to write.)"))
						]
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(SSeparator)
			]

			// -------- Approval bar --------
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(EditButton, SButton)
					.Text(LOCTEXT("VesselEditButton", "Edit"))
					.IsEnabled(false)
					.OnClicked(FOnClicked::CreateSP(this, &SVesselChatPanel::HandleEditClicked))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.f, 0.f, 0.f, 0.f)
				[
					SAssignNew(RejectButton, SButton)
					.Text(LOCTEXT("VesselRejectButton", "Reject with reason"))
					.IsEnabled(false)
					.OnClicked(FOnClicked::CreateSP(this, &SVesselChatPanel::HandleRejectClicked))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)  // push Approve to the right
				[
					SNew(SSpacer)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(ApproveButton, SButton)
					.Text(LOCTEXT("VesselApproveButton", "Approve && Execute"))
					.IsEnabled(false)
					.OnClicked(FOnClicked::CreateSP(this, &SVesselChatPanel::HandleApproveClicked))
				]
			]
		]
	];
}

void SVesselChatPanel::AppendMessageInternal(const FString& Prefix, const FString& Text)
{
	if (!ChatScroll.IsValid())
	{
		return;
	}
	ChatScroll->AddSlot()
	.Padding(2.f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(Prefix + Text))
		.AutoWrapText(true)
	];
	ChatScroll->ScrollToEnd();
}

void SVesselChatPanel::AppendUserMessage(const FString& Text)
{
	AppendMessageInternal(TEXT("you · "), Text);
}

void SVesselChatPanel::AppendAssistantMessage(const FString& Text)
{
	AppendMessageInternal(TEXT("Vessel · "), Text);
}

void SVesselChatPanel::SetAgentStatus(const FString& StatusLabel)
{
	if (AgentStatus.IsValid())
	{
		AgentStatus->SetText(FText::FromString(StatusLabel));
	}
}

void SVesselChatPanel::SetDiffPreview(const FString& DiffText)
{
	if (DiffArea.IsValid())
	{
		DiffArea->SetText(FText::FromString(DiffText));
	}
}

void SVesselChatPanel::SetCostLabel(const FString& CostText)
{
	if (CostLabel.IsValid())
	{
		CostLabel->SetText(FText::FromString(CostText));
	}
}

// =============================================================================
// Handlers — Step 4c.1 stubs. Session wiring lands in 4c.2.
// =============================================================================

FReply SVesselChatPanel::HandleSendClicked()
{
	if (!InputBox.IsValid())
	{
		return FReply::Handled();
	}
	const FString Text = InputBox->GetText().ToString().TrimStartAndEnd();
	if (Text.IsEmpty())
	{
		return FReply::Handled();
	}
	AppendUserMessage(Text);
	AppendAssistantMessage(TEXT("(Session wiring lands in Step 4c.2 — your input was logged to the chat panel but not yet dispatched.)"));
	InputBox->SetText(FText());
	UE_LOG(LogVesselHITL, Log, TEXT("ChatPanel Send: %s"), *Text);
	return FReply::Handled();
}

FReply SVesselChatPanel::HandleApproveClicked()
{
	UE_LOG(LogVesselHITL, Log, TEXT("ChatPanel Approve clicked (skeleton stub)"));
	return FReply::Handled();
}

FReply SVesselChatPanel::HandleRejectClicked()
{
	UE_LOG(LogVesselHITL, Log, TEXT("ChatPanel Reject clicked (skeleton stub)"));
	return FReply::Handled();
}

FReply SVesselChatPanel::HandleEditClicked()
{
	UE_LOG(LogVesselHITL, Log, TEXT("ChatPanel Edit clicked (skeleton stub)"));
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
