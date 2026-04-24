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

class FVesselSessionMachine;
class FVesselSlateApprovalClient;
struct FVesselSessionOutcome;

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

	// --- Helpers ---
	void AppendMessageInternal(const FString& Prefix, const FString& Text);
	void SetApprovalButtonsEnabled(bool bEnabled);

	// Which view of the action bar is showing.
	enum class EBarView : uint8
	{
		Normal,         // Edit / Reject / Approve
		RejectReason,   // reason input + Confirm/Cancel
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

	// --- Session state ---
	TSharedPtr<FVesselSessionMachine>     CurrentSession;
	TSharedPtr<FVesselSlateApprovalClient> ApprovalClient;
	TOptional<FVesselApprovalRequest>     PendingRequest;
	TSharedPtr<TPromise<FVesselApprovalDecision>> PendingPromise;
};
