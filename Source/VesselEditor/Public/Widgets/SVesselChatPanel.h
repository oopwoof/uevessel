// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Session/VesselApprovalTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class STextBlock;
class SEditableTextBox;
class SMultiLineEditableTextBox;
class SButton;
class SScrollBox;
class SWidgetSwitcher;
class SWidget;

class FVesselSessionMachine;
class FVesselSlateApprovalClient;
struct FVesselSessionOutcome;
struct FVesselPlan;
struct FVesselPlanStep;
struct FVesselJudgeVerdict;

/**
 * Top-level Vessel dock panel. Owns the per-tab session machine, a Slate
 * approval client, and the HITL UI. See ARCHITECTURE.md §4.1 (native Slate
 * SDockTab, not EUW) and HITL_PROTOCOL.md §2.2 for the approval contract.
 */
// SCompoundWidget already derives from SWidget which is TSharedFromThis<SWidget>;
// an explicit TSharedFromThis<SVesselChatPanel> would make AsShared() ambiguous.
// `SharedThis(this)` works correctly as-is via SWidget's built-in support.
class VESSELEDITOR_API SVesselChatPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVesselChatPanel) {}
	SLATE_END_ARGS()

	virtual ~SVesselChatPanel() override;

	void Construct(const FArguments& InArgs);

	// Public API for future bridge / external drivers.
	void AppendUserMessage(const FString& Text);
	void AppendAssistantMessage(const FString& Text);
	void SetAgentStatus(const FString& StatusLabel);
	void SetDiffPreview(const FString& DiffText);
	void SetCostLabel(const FString& CostText);

private:
	// --- Session lifecycle ---
	void BeginSession(const FString& UserInput);
	void OnSessionComplete(const FVesselSessionOutcome& Outcome);

	// --- HITL ---
	void HandleApprovalRequested(
		const FVesselApprovalRequest& Request,
		TSharedRef<TPromise<FVesselApprovalDecision>> Promise);
	void EnterApprovalMode(const FVesselApprovalRequest& Request);
	void LeaveApprovalMode();
	void EnterRejectReasonMode();
	void LeaveRejectReasonMode();

	// --- Input handlers ---
	FReply HandleSendClicked();
	FReply HandleApproveClicked();
	FReply HandleRejectClicked();
	FReply HandleEditClicked();
	FReply HandleConfirmRejectClicked();
	FReply HandleCancelRejectClicked();
	FReply HandleConfirmEditClicked();
	FReply HandleCancelEditClicked();

	// Edit-and-approve mode lifecycle.
	void EnterEditArgsMode();
	void LeaveEditArgsMode();

	// --- Helpers ---
	void AppendMessageInternal(const FString& Prefix, const FString& Text);
	void AppendChatWidget(TSharedRef<SWidget> W);
	void SetApprovalButtonsEnabled(bool bEnabled);

	/**
	 * Session-event card append helpers — bound to FVesselSessionMachine's
	 * observation delegates (OnPlanReady / OnStepExecuted / OnJudgeVerdict).
	 * Always called on the Game Thread.
	 */
	void HandlePlanReady(const FVesselPlan& Plan);
	void HandleStepExecuted(
		const FVesselPlanStep& Step, const FString& ResultJson,
		bool bWasError, const FString& ErrorMessage);
	void HandleJudgeVerdict(const FVesselJudgeVerdict& Verdict);

	/** Read TotalCostUsd from the live session and format it into the header label. */
	void RefreshCostFromSession();

	// Which view of the action bar is showing.
	enum class EBarView : uint8
	{
		Normal,         // Edit / Reject / Approve
		RejectReason,   // reason input + Confirm/Cancel
		EditArgs,       // args JSON input + Confirm Edit / Cancel
	};
	void SetBarView(EBarView View);

private:
	// --- Widget refs ---
	TSharedPtr<SEditableTextBox>          InputBox;
	TSharedPtr<SButton>                   SendButton;
	TSharedPtr<SScrollBox>                ChatScroll;
	TSharedPtr<STextBlock>                AgentStatus;
	TSharedPtr<STextBlock>                CostLabelWidget;
	TSharedPtr<SMultiLineEditableTextBox> DiffArea;
	TSharedPtr<SButton>                   ApproveButton;
	TSharedPtr<SButton>                   RejectButton;
	TSharedPtr<SButton>                   EditButton;
	TSharedPtr<SWidgetSwitcher>           BarSwitcher;
	TSharedPtr<SMultiLineEditableTextBox> RejectReasonInput;
	TSharedPtr<SMultiLineEditableTextBox> EditArgsInput;

	// --- Session state ---
	TSharedPtr<FVesselSessionMachine>     CurrentSession;
	TSharedPtr<FVesselSlateApprovalClient> ApprovalClient;
	TOptional<FVesselApprovalRequest>     PendingRequest;
	TSharedPtr<TPromise<FVesselApprovalDecision>> PendingPromise;

	/**
	 * Two-click guard for short reject reasons. First Confirm-Reject click
	 * with a reason ≤5 chars surfaces a warning + sets this flag; the second
	 * click submits even with the short reason. Reset whenever reject-reason
	 * mode is entered/left so each new reject starts clean.
	 */
	bool bShortReasonAcknowledged = false;
};
