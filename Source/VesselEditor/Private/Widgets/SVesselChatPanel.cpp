// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Widgets/SVesselChatPanel.h"

#include "VesselLog.h"
#include "Session/VesselAgentTemplates.h"
#include "Session/VesselApprovalClient.h"
#include "Session/VesselSessionConfig.h"
#include "Session/VesselSessionMachine.h"
#include "Session/VesselSessionTypes.h"
#include "Util/VesselJsonSanitizer.h"
#include "Widgets/SVesselPlanCard.h"
#include "Widgets/SVesselResultCard.h"
#include "Widgets/SVesselVerdictCard.h"
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
					.Text(LOCTEXT("VesselCostPlaceholder", "$0.0000 (est)"))
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
							"Edit the args JSON before approving — useful for tweaking a row "
							"value or fixing a typo without rejecting + re-prompting."))
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
							"Why are you rejecting? This is written to AGENTS.md and shown "
							"to the agent on its next turn — concrete reasons help the agent "
							"learn the project's policies."))
						.AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(STextBlock).Text(LOCTEXT("VesselRejectReasonExamples",
							"Examples:  • RowName violates the NPC_<Region>_<Type> convention\n"
							"           • would overwrite production-balanced stats without VCS backup\n"
							"           • Title is hardcoded Chinese — must go through LOC pipeline"))
						.ColorAndOpacity(FLinearColor(0.65f, 0.65f, 0.65f, 1.0f))
						.AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SAssignNew(RejectReasonInput, SMultiLineEditableTextBox)
						.AllowMultiLine(true)
						.HintText(LOCTEXT("VesselRejectReasonHint",
							"Describe what's wrong — at least one full sentence is best."))
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

				// [2] Edit-args input — Edit-and-Approve path
				+ SWidgetSwitcher::Slot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(STextBlock).Text(LOCTEXT("VesselEditArgsPrompt",
							"Edit the args JSON. The agent's plan reasoning still applies — "
							"you're only adjusting tool inputs. Must remain valid JSON."))
						.AutoWrapText(true)
					]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SAssignNew(EditArgsInput, SMultiLineEditableTextBox)
						.AllowMultiLine(true)
						.HintText(LOCTEXT("VesselEditArgsHint", "Edited args JSON..."))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f) [ SNew(SSpacer) ]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton)
							.Text(LOCTEXT("VesselCancelEditButton", "Cancel"))
							.OnClicked(FOnClicked::CreateSP(this, &SVesselChatPanel::HandleCancelEditClicked))
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f)
						[
							SNew(SButton)
							.Text(LOCTEXT("VesselConfirmEditButton", "Confirm Edit && Execute"))
							.OnClicked(FOnClicked::CreateSP(this, &SVesselChatPanel::HandleConfirmEditClicked))
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

void SVesselChatPanel::AppendChatWidget(TSharedRef<SWidget> W)
{
	if (!ChatScroll.IsValid()) { return; }

	// Cap total widgets so a long session doesn't accumulate hundreds of cards.
	while (ChatScroll->GetChildren()->Num() >= VesselChatPanelDetail::MaxChatMessages)
	{
		ChatScroll->RemoveSlot(ChatScroll->GetChildren()->GetChildAt(0));
	}

	ChatScroll->AddSlot().Padding(2.f) [ W ];
	ChatScroll->ScrollToEnd();
}

void SVesselChatPanel::AppendMessageInternal(const FString& Prefix, const FString& Text)
{
	AppendChatWidget(
		SNew(STextBlock)
		.Text(FText::FromString(Prefix + Text))
		.AutoWrapText(true));
}

void SVesselChatPanel::AppendUserMessage(const FString& Text)      { AppendMessageInternal(TEXT("you · "), Text); }
void SVesselChatPanel::AppendAssistantMessage(const FString& Text) { AppendMessageInternal(TEXT("Vessel · "), Text); }

void SVesselChatPanel::HandlePlanReady(const FVesselPlan& Plan)
{
	AppendChatWidget(SNew(SVesselPlanCard).Plan(&Plan));
	RefreshCostFromSession();
}

void SVesselChatPanel::HandleStepExecuted(
	const FVesselPlanStep& Step, const FString& ResultJson,
	bool bWasError, const FString& ErrorMessage)
{
	AppendChatWidget(
		SNew(SVesselResultCard)
		.StepIndex(Step.StepIndex)
		.ToolName(Step.ToolName)
		.ResultJson(ResultJson)
		.bWasError(bWasError)
		.ErrorMessage(ErrorMessage));
}

void SVesselChatPanel::HandleJudgeVerdict(const FVesselJudgeVerdict& Verdict)
{
	AppendChatWidget(SNew(SVesselVerdictCard).Verdict(&Verdict));
	RefreshCostFromSession();
}

void SVesselChatPanel::RefreshCostFromSession()
{
	if (!CurrentSession.IsValid()) { return; }
	const double Cost = CurrentSession->GetTotalCostUsd();
	// Estimates from a snapshot pricing table — always show 4 fractional digits
	// so micro-cost runs ($0.0042 typical for one Sonnet+Haiku step) don't
	// round to $0.00. Append "(est)" so it never reads like a billed total.
	SetCostLabel(FString::Printf(TEXT("$%.4f (est)"), Cost));
}

void SVesselChatPanel::SetAgentStatus(const FString& S)  { if (AgentStatus.IsValid())     AgentStatus->SetText(FText::FromString(S)); }
void SVesselChatPanel::SetDiffPreview(const FString& D)  { if (DiffArea.IsValid())        DiffArea->SetText(FText::FromString(D)); }
void SVesselChatPanel::SetCostLabel(const FString& C)    { if (CostLabelWidget.IsValid()) CostLabelWidget->SetText(FText::FromString(C)); }

void SVesselChatPanel::SetApprovalButtonsEnabled(bool bEnabled)
{
	if (ApproveButton.IsValid()) ApproveButton->SetEnabled(bEnabled);
	if (RejectButton.IsValid())  RejectButton->SetEnabled(bEnabled);
	if (EditButton.IsValid())    EditButton->SetEnabled(bEnabled);
}

void SVesselChatPanel::SetBarView(EBarView View)
{
	if (!BarSwitcher.IsValid()) { return; }
	int32 Idx = 0;
	switch (View)
	{
		case EBarView::Normal:        Idx = 0; break;
		case EBarView::RejectReason:  Idx = 1; break;
		case EBarView::EditArgs:      Idx = 2; break;
	}
	BarSwitcher->SetActiveWidgetIndex(Idx);
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
	SetCostLabel(TEXT("$0.0000 (est)")); // reset previous session's running total

	FVesselSessionConfig Config = MakeDefaultSessionConfig(FString());
	// Override the minimal fallback template with the shipping Designer Assistant.
	Config.AgentTemplate = FVesselAgentTemplates::MakeDesignerAssistant();
	if (Config.ProviderId.IsEmpty())
	{
		Config.ProviderId = TEXT("mock");
	}

	CurrentSession = MakeShared<FVesselSessionMachine>();

	ApprovalClient = MakeShared<FVesselSlateApprovalClient>();
	TWeakPtr<SVesselChatPanel> WeakSelf = SharedThis(this);
	ApprovalClient->OnApprovalRequested.BindLambda(
		[WeakSelf](const FVesselApprovalRequest& Request,
		           TSharedRef<TPromise<FVesselApprovalDecision>> Promise)
		{
			if (TSharedPtr<SVesselChatPanel> Pinned = WeakSelf.Pin())
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

	// Subscribe to session observation delegates so the chat panel surfaces
	// each FSM event as a card. Delegates fire on the Game Thread per the
	// Session Machine contract, so handlers can touch Slate directly.
	// WeakSelf prevents firing into a destroyed panel.
	CurrentSession->OnPlanReady.AddLambda(
		[WeakSelf](const FVesselPlan& Plan)
		{
			if (TSharedPtr<SVesselChatPanel> Pinned = WeakSelf.Pin())
			{
				Pinned->HandlePlanReady(Plan);
			}
		});
	CurrentSession->OnStepExecuted.AddLambda(
		[WeakSelf](const FVesselPlanStep& Step, const FString& ResultJson,
			bool bWasError, const FString& ErrorMessage)
		{
			if (TSharedPtr<SVesselChatPanel> Pinned = WeakSelf.Pin())
			{
				Pinned->HandleStepExecuted(Step, ResultJson, bWasError, ErrorMessage);
			}
		});
	CurrentSession->OnJudgeVerdict.AddLambda(
		[WeakSelf](const FVesselJudgeVerdict& Verdict)
		{
			if (TSharedPtr<SVesselChatPanel> Pinned = WeakSelf.Pin())
			{
				Pinned->HandleJudgeVerdict(Verdict);
			}
		});

	CurrentSession->RunAsync(UserInput).Next(
		[WeakSelf](FVesselSessionOutcome Outcome)
		{
			AsyncTask(ENamedThreads::GameThread,
				[WeakSelf, Out = MoveTemp(Outcome)]() mutable
				{
					if (TSharedPtr<SVesselChatPanel> Pinned = WeakSelf.Pin())
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

	SetCostLabel(FString::Printf(TEXT("$%.4f (est)"), Outcome.TotalCostUsd));
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
	bShortReasonAcknowledged = false;
	SetBarView(EBarView::RejectReason);
}

void SVesselChatPanel::LeaveRejectReasonMode()
{
	bShortReasonAcknowledged = false;
	SetBarView(EBarView::Normal);
}

void SVesselChatPanel::EnterEditArgsMode()
{
	// Pre-fill the edit box with the agent's original args JSON so the user
	// only has to tweak. PendingRequest is the live approval payload.
	if (EditArgsInput.IsValid())
	{
		const FString Initial = PendingRequest.IsSet()
			? PendingRequest->ArgsJson
			: FString();
		EditArgsInput->SetText(FText::FromString(Initial));
	}
	SetBarView(EBarView::EditArgs);
}

void SVesselChatPanel::LeaveEditArgsMode()
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

	// Hard floor: empty reasons are useless to AGENTS.md and to the next-turn
	// LLM. Block them outright.
	if (Reason.IsEmpty())
	{
		AppendAssistantMessage(TEXT("Reject reason cannot be empty."));
		return FReply::Handled();
	}

	// Soft warning: reasons ≤5 chars rarely teach the agent anything ("ok",
	// "no", "测试"). First click warns + sets the ack flag; second click
	// submits anyway (user has acknowledged the reason is thin).
	if (Reason.Len() <= 5 && !bShortReasonAcknowledged)
	{
		bShortReasonAcknowledged = true;
		AppendAssistantMessage(FString::Printf(
			TEXT("Reason '%s' is short (%d chars). Concrete reasons help the agent "
			     "learn project policies — see the examples above. Click Confirm "
			     "Reject again to submit anyway."),
			*Reason, Reason.Len()));
		return FReply::Handled();
	}

	PendingPromise->SetValue(FVesselApprovalDecision::MakeReject(Reason, TEXT("user")));
	PendingPromise.Reset();
	PendingRequest.Reset();
	bShortReasonAcknowledged = false;
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
	if (!PendingPromise.IsValid() || !PendingRequest.IsSet())
	{
		// No pending approval — nothing to edit. Defensive against stale clicks.
		return FReply::Handled();
	}
	EnterEditArgsMode();
	return FReply::Handled();
}

FReply SVesselChatPanel::HandleConfirmEditClicked()
{
	if (!PendingPromise.IsValid() || !EditArgsInput.IsValid())
	{
		return FReply::Handled();
	}
	const FString EditedJson = EditArgsInput->GetText().ToString().TrimStartAndEnd();
	if (EditedJson.IsEmpty())
	{
		AppendAssistantMessage(TEXT("Edit failed: args JSON cannot be empty."));
		return FReply::Handled();
	}

	// Validate the user's edit parses as a JSON object before submitting.
	// FVesselJsonSanitizer::ParseAsObject runs the same fence-strip + repair
	// pass the agent's own JSON goes through, so an LLM-flavored hand edit
	// (e.g. trailing newline, embedded ```json fence) still passes.
	TSharedPtr<FJsonObject> Parsed;
	if (!FVesselJsonSanitizer::ParseAsObject(EditedJson, Parsed) || !Parsed.IsValid())
	{
		AppendAssistantMessage(TEXT(
			"Edit failed: args JSON did not parse. Fix syntax and try again — "
			"the original plan stays pending until you Confirm or Cancel."));
		return FReply::Handled();
	}

	PendingPromise->SetValue(FVesselApprovalDecision::MakeEdit(EditedJson, TEXT("user")));
	PendingPromise.Reset();
	PendingRequest.Reset();
	LeaveEditArgsMode();
	LeaveApprovalMode();
	SetAgentStatus(TEXT("Agent: executing edited step..."));
	return FReply::Handled();
}

FReply SVesselChatPanel::HandleCancelEditClicked()
{
	LeaveEditArgsMode();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
