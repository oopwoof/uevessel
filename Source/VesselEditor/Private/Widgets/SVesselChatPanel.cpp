// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Widgets/SVesselChatPanel.h"

#include "VesselLog.h"
#include "Session/VesselAgentTemplates.h"
#include "Session/VesselApprovalClient.h"
#include "Session/VesselSessionConfig.h"
#include "Session/VesselSessionMachine.h"
#include "Session/VesselSessionTypes.h"
#include "Widgets/VesselSlateApprovalClient.h"

#include "Async/Async.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "VesselChatPanel"

namespace VesselChatPanelDetail
{
	/** Cap on how many message widgets we keep in ChatScroll — prevents long sessions from bloating Slate. */
	static constexpr int32 MaxChatMessages = 200;

	/** Best-effort short rendering of approval request for the diff area. */
	static FString BuildApprovalSummary(const FVesselApprovalRequest& Request)
	{
		FString Summary;
		Summary += FString::Printf(TEXT("Step %d · tool=%s\n"),
			Request.StepIndex, *Request.ToolName.ToString());
		if (!Request.ToolCategory.IsEmpty())
		{
			Summary += FString::Printf(TEXT("category: %s\n"), *Request.ToolCategory);
		}
		if (Request.bIrreversibleHint)
		{
			Summary += TEXT("\n⚠ This change cannot be undone via Ctrl+Z. Ensure VCS backup exists.\n");
		}
		if (!Request.Reasoning.IsEmpty())
		{
			Summary += FString::Printf(TEXT("\nAgent reasoning:\n%s\n"), *Request.Reasoning);
		}
		Summary += FString::Printf(TEXT("\nArgs JSON:\n%s\n"), *Request.ArgsJson);
		return Summary;
	}
}

SVesselChatPanel::~SVesselChatPanel()
{
	// CRITICAL: if the tab is closed while an approval is pending, the session
	// machine is waiting on a future that would never resolve without this.
	// Fulfill as Reject with a clear reason; the session's normal reject path
	// then terminates the session gracefully.
	if (PendingPromise.IsValid())
	{
		PendingPromise->SetValue(FVesselApprovalDecision::MakeReject(
			TEXT("Vessel panel was closed while an approval was pending."),
			TEXT("slate-panel:dtor")));
		PendingPromise.Reset();
	}
	PendingRequest.Reset();
	// Release the session machine; its destructor unregisters its own
	// OnEditorClose handle and closes the JSONL log.
	CurrentSession.Reset();
	ApprovalClient.Reset();
}

void SVesselChatPanel::Construct(const FArguments& /*InArgs*/)
{
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Docking.Tab.ContentAreaBrush"))
		.Padding(FMargin(8.f))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 6.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("VesselHeaderLabel", "🔷 Vessel"))
					.TextStyle(FAppStyle::Get(), "HeaderText")
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(12.f, 0.f)
				[
					SAssignNew(AgentStatus, STextBlock)
					.Text(LOCTEXT("VesselIdleStatus", "Agent: idle"))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SAssignNew(CostLabelWidget, STextBlock)
					.Text(LOCTEXT("VesselCostPlaceholder", "$0.00"))
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f) [ SNew(SSeparator) ]

			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SSplitter).Orientation(Orient_Vertical)

				+ SSplitter::Slot().Value(0.6f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
						.Padding(4.f)
						[
							SAssignNew(ChatScroll, SScrollBox)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							SAssignNew(InputBox, SEditableTextBox)
							.HintText(LOCTEXT("VesselInputHint",
								"Describe what you want the agent to do..."))
							.OnTextCommitted_Lambda([this](const FText& /*T*/, ETextCommit::Type Commit)
							{
								if (Commit == ETextCommit::OnEnter) { HandleSendClicked(); }
							})
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f, 0.f, 0.f)
						[
							SAssignNew(SendButton, SButton)
							.Text(LOCTEXT("VesselSendButton", "Send"))
							.OnClicked(FOnClicked::CreateSP(this, &SVesselChatPanel::HandleSendClicked))
						]
					]
				]

				+ SSplitter::Slot().Value(0.4f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(4.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("VesselDiffLabel", "Diff preview / Agent reasoning"))
							.TextStyle(FAppStyle::Get(), "SmallText")
						]
						+ SVerticalBox::Slot().FillHeight(1.f)
						[
							SAssignNew(DiffArea, SMultiLineEditableTextBox)
							.IsReadOnly(true)
							.Text(LOCTEXT("VesselDiffEmpty", "(No pending change.)"))
						]
					]
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f) [ SNew(SSeparator) ]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 6.f, 0.f, 0.f)
			[
				SAssignNew(BarSwitcher, SWidgetSwitcher)
				.WidgetIndex(0)

				// [0] Normal action bar
				+ SWidgetSwitcher::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SAssignNew(EditButton, SButton)
						.Text(LOCTEXT("VesselEditButton", "Edit"))
						.IsEnabled(false)
						.ToolTipText(LOCTEXT("VesselEditButtonTooltip",
							"Edit args before approving — arrives in Step 4c.3."))
						.OnClicked(FOnClicked::CreateSP(this, &SVesselChatPanel::HandleEditClicked))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f)
					[
						SAssignNew(RejectButton, SButton)
						.Text(LOCTEXT("VesselRejectButton", "Reject with reason"))
						.IsEnabled(false)
						.OnClicked(FOnClicked::CreateSP(this, &SVesselChatPanel::HandleRejectClicked))
					]
					+ SHorizontalBox::Slot().FillWidth(1.f) [ SNew(SSpacer) ]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SAssignNew(ApproveButton, SButton)
						.Text(LOCTEXT("VesselApproveButton", "Approve && Execute"))
						.IsEnabled(false)
						.OnClicked(FOnClicked::CreateSP(this, &SVesselChatPanel::HandleApproveClicked))
					]
				]

				// [1] Reject-reason input
				+ SWidgetSwitcher::Slot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(STextBlock).Text(LOCTEXT("VesselRejectReasonPrompt",
							"Reject reason (min 5 chars). Will be written to AGENTS.md."))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SAssignNew(RejectReasonInput, SMultiLineEditableTextBox)
						.AllowMultiLine(true)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f) [ SNew(SSpacer) ]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton)
							.Text(LOCTEXT("VesselCancelRejectButton", "Cancel"))
							.OnClicked(FOnClicked::CreateSP(this, &SVesselChatPanel::HandleCancelRejectClicked))
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f)
						[
							SNew(SButton)
							.Text(LOCTEXT("VesselConfirmRejectButton", "Confirm Reject"))
							.OnClicked(FOnClicked::CreateSP(this, &SVesselChatPanel::HandleConfirmRejectClicked))
						]
					]
				]
			]
		]
	];
}

// =============================================================================
// Public API
// =============================================================================

void SVesselChatPanel::AppendMessageInternal(const FString& Prefix, const FString& Text)
{
	if (!ChatScroll.IsValid()) { return; }

	// Cap total messages so a long session doesn't accumulate thousands of widgets.
	while (ChatScroll->GetChildren()->Num() >= VesselChatPanelDetail::MaxChatMessages)
	{
		ChatScroll->RemoveSlot(ChatScroll->GetChildren()->GetChildAt(0));
	}

	ChatScroll->AddSlot().Padding(2.f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(Prefix + Text))
		.AutoWrapText(true)
	];
	ChatScroll->ScrollToEnd();
}

void SVesselChatPanel::AppendUserMessage(const FString& Text)      { AppendMessageInternal(TEXT("you · "), Text); }
void SVesselChatPanel::AppendAssistantMessage(const FString& Text) { AppendMessageInternal(TEXT("Vessel · "), Text); }

void SVesselChatPanel::SetAgentStatus(const FString& S)  { if (AgentStatus.IsValid())     AgentStatus->SetText(FText::FromString(S)); }
void SVesselChatPanel::SetDiffPreview(const FString& D)  { if (DiffArea.IsValid())        DiffArea->SetText(FText::FromString(D)); }
void SVesselChatPanel::SetCostLabel(const FString& C)    { if (CostLabelWidget.IsValid()) CostLabelWidget->SetText(FText::FromString(C)); }

void SVesselChatPanel::SetApprovalButtonsEnabled(bool bEnabled)
{
	if (ApproveButton.IsValid()) ApproveButton->SetEnabled(bEnabled);
	if (RejectButton.IsValid())  RejectButton->SetEnabled(bEnabled);
	// Edit stays disabled in Step 4c.2.
}

void SVesselChatPanel::SetBarView(EBarView View)
{
	if (BarSwitcher.IsValid())
	{
		BarSwitcher->SetActiveWidgetIndex(View == EBarView::Normal ? 0 : 1);
	}
}

// =============================================================================
// Session lifecycle
// =============================================================================

void SVesselChatPanel::BeginSession(const FString& UserInput)
{
	if (CurrentSession.IsValid())
	{
		AppendAssistantMessage(TEXT("A session is already running. Wait for it to finish or abort it."));
		return;
	}

	SetApprovalButtonsEnabled(false);
	SetBarView(EBarView::Normal);
	if (SendButton.IsValid()) { SendButton->SetEnabled(false); }
	if (InputBox.IsValid())   { InputBox->SetEnabled(false); }

	SetAgentStatus(TEXT("Agent: planning..."));
	SetDiffPreview(TEXT("(Agent is thinking — diff will appear when a tool wants to run.)"));

	FVesselSessionConfig Config = MakeDefaultSessionConfig(FString());
	// Override the minimal fallback template with the shipping Designer Assistant.
	Config.AgentTemplate = FVesselAgentTemplates::MakeDesignerAssistant();
	if (Config.ProviderId.IsEmpty())
	{
		Config.ProviderId = TEXT("mock");
	}

	CurrentSession = MakeShared<FVesselSessionMachine>();

	ApprovalClient = MakeShared<FVesselSlateApprovalClient>();
	TWeakPtr<SVesselChatPanel> WeakThis = SharedThis(this);
	ApprovalClient->OnApprovalRequested.BindLambda(
		[WeakThis](const FVesselApprovalRequest& Request,
		           TSharedRef<TPromise<FVesselApprovalDecision>> Promise)
		{
			if (TSharedPtr<SVesselChatPanel> Pinned = WeakThis.Pin())
			{
				Pinned->HandleApprovalRequested(Request, Promise);
			}
			else
			{
				Promise->SetValue(FVesselApprovalDecision::MakeReject(
					TEXT("Vessel panel closed during approval wait."),
					TEXT("slate-client:panel-gone")));
			}
		});

	CurrentSession->SetApprovalClient(ApprovalClient.ToSharedRef());
	if (!CurrentSession->Init(Config))
	{
		AppendAssistantMessage(TEXT("Session failed to initialize. Check Output Log / Vessel settings."));
		SetAgentStatus(TEXT("Agent: idle"));
		CurrentSession.Reset();
		ApprovalClient.Reset();
		if (SendButton.IsValid()) { SendButton->SetEnabled(true); }
		if (InputBox.IsValid())   { InputBox->SetEnabled(true); }
		return;
	}

	CurrentSession->RunAsync(UserInput).Next(
		[WeakThis](FVesselSessionOutcome Outcome)
		{
			AsyncTask(ENamedThreads::GameThread,
				[WeakThis, Out = MoveTemp(Outcome)]() mutable
				{
					if (TSharedPtr<SVesselChatPanel> Pinned = WeakThis.Pin())
					{
						Pinned->OnSessionComplete(Out);
					}
				});
		});
}

void SVesselChatPanel::OnSessionComplete(const FVesselSessionOutcome& Outcome)
{
	switch (Outcome.Kind)
	{
		case EVesselSessionOutcomeKind::Done:
			AppendAssistantMessage(Outcome.FinalAssistantText.IsEmpty()
				? FString(TEXT("Session complete."))
				: Outcome.FinalAssistantText);
			SetAgentStatus(FString::Printf(TEXT("Agent: done · %d step(s)"), Outcome.StepsExecuted));
			break;
		case EVesselSessionOutcomeKind::Failed:
		case EVesselSessionOutcomeKind::AbortedByUser:
		case EVesselSessionOutcomeKind::AbortedOnEditorClose:
			AppendAssistantMessage(FString::Printf(TEXT("Session ended: %s"), *Outcome.Reason));
			SetAgentStatus(FString::Printf(TEXT("Agent: %s"),
				SessionOutcomeKindToString(Outcome.Kind)));
			break;
		default: break;
	}

	SetCostLabel(FString::Printf(TEXT("$%.2f"), Outcome.TotalCostUsd));
	SetApprovalButtonsEnabled(false);
	SetBarView(EBarView::Normal);
	SetDiffPreview(TEXT("(No pending change.)"));

	if (SendButton.IsValid()) { SendButton->SetEnabled(true); }
	if (InputBox.IsValid())   { InputBox->SetEnabled(true); }

	CurrentSession.Reset();
	ApprovalClient.Reset();
	PendingRequest.Reset();
	PendingPromise.Reset();
}

// =============================================================================
// HITL
// =============================================================================

void SVesselChatPanel::HandleApprovalRequested(
	const FVesselApprovalRequest& Request,
	TSharedRef<TPromise<FVesselApprovalDecision>> Promise)
{
	PendingRequest = Request;
	PendingPromise = Promise;
	EnterApprovalMode(Request);
}

void SVesselChatPanel::EnterApprovalMode(const FVesselApprovalRequest& Request)
{
	SetAgentStatus(FString::Printf(TEXT("Agent: awaiting approval · step %d · %s"),
		Request.StepIndex, *Request.ToolName.ToString()));
	SetDiffPreview(VesselChatPanelDetail::BuildApprovalSummary(Request));
	SetBarView(EBarView::Normal);
	SetApprovalButtonsEnabled(true);
}

void SVesselChatPanel::LeaveApprovalMode()
{
	SetApprovalButtonsEnabled(false);
	SetBarView(EBarView::Normal);
	SetDiffPreview(TEXT("(No pending change.)"));
}

void SVesselChatPanel::EnterRejectReasonMode()
{
	if (RejectReasonInput.IsValid())
	{
		RejectReasonInput->SetText(FText());
	}
	SetBarView(EBarView::RejectReason);
}

void SVesselChatPanel::LeaveRejectReasonMode()
{
	SetBarView(EBarView::Normal);
}

// =============================================================================
// Handlers
// =============================================================================

FReply SVesselChatPanel::HandleSendClicked()
{
	if (!InputBox.IsValid()) { return FReply::Handled(); }
	const FString Text = InputBox->GetText().ToString().TrimStartAndEnd();
	if (Text.IsEmpty()) { return FReply::Handled(); }

	AppendUserMessage(Text);
	InputBox->SetText(FText());
	BeginSession(Text);
	return FReply::Handled();
}

FReply SVesselChatPanel::HandleApproveClicked()
{
	if (PendingPromise.IsValid())
	{
		PendingPromise->SetValue(FVesselApprovalDecision::MakeApprove(TEXT("user")));
		PendingPromise.Reset();
		PendingRequest.Reset();
		LeaveApprovalMode();
		SetAgentStatus(TEXT("Agent: executing..."));
	}
	return FReply::Handled();
}

FReply SVesselChatPanel::HandleRejectClicked()
{
	EnterRejectReasonMode();
	return FReply::Handled();
}

FReply SVesselChatPanel::HandleConfirmRejectClicked()
{
	if (!PendingPromise.IsValid() || !RejectReasonInput.IsValid())
	{
		return FReply::Handled();
	}
	const FString Reason = RejectReasonInput->GetText().ToString().TrimStartAndEnd();
	if (Reason.Len() < 5)
	{
		AppendAssistantMessage(TEXT("Reject reason must be at least 5 characters."));
		return FReply::Handled();
	}
	PendingPromise->SetValue(FVesselApprovalDecision::MakeReject(Reason, TEXT("user")));
	PendingPromise.Reset();
	PendingRequest.Reset();
	LeaveRejectReasonMode();
	LeaveApprovalMode();
	SetAgentStatus(TEXT("Agent: rejected — finishing..."));
	return FReply::Handled();
}

FReply SVesselChatPanel::HandleCancelRejectClicked()
{
	LeaveRejectReasonMode();
	return FReply::Handled();
}

FReply SVesselChatPanel::HandleEditClicked()
{
	UE_LOG(LogVesselHITL, Log, TEXT("ChatPanel Edit clicked (disabled in Step 4c.2)"));
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
