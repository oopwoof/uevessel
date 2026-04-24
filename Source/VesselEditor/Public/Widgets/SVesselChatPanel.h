// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class STextBlock;
class SEditableTextBox;
class SMultiLineEditableTextBox;
class SButton;
class SScrollBox;

/**
 * The main Vessel dock panel: a native Slate SDockTab content widget with
 * three stacked regions — agent status header, chat/diff split, and an
 * approval action bar. Step 4c.1 is a visual skeleton: all state is local,
 * callbacks are stubs. Step 4c.2 wires this to FVesselSessionMachine via a
 * Slate-hosted IVesselApprovalClient.
 *
 * Not built on Editor Utility Widget (UMG) — see ARCHITECTURE.md §4.1 ADR.
 */
class VESSELEDITOR_API SVesselChatPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVesselChatPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// Minimal public API for future session integration.
	void AppendUserMessage(const FString& Text);
	void AppendAssistantMessage(const FString& Text);
	void SetAgentStatus(const FString& StatusLabel);
	void SetDiffPreview(const FString& DiffText);
	void SetCostLabel(const FString& CostText);

private:
	// --- Handlers (stubs in Step 4c.1) ---
	FReply HandleSendClicked();
	FReply HandleApproveClicked();
	FReply HandleRejectClicked();
	FReply HandleEditClicked();

	void AppendMessageInternal(const FString& Prefix, const FString& Text);

	// --- Widget refs (held so we can mutate state cheaply) ---
	TSharedPtr<SEditableTextBox>   InputBox;
	TSharedPtr<SScrollBox>         ChatScroll;
	TSharedPtr<STextBlock>         AgentStatus;
	TSharedPtr<STextBlock>         CostLabel;
	TSharedPtr<SMultiLineEditableTextBox> DiffArea;
	TSharedPtr<SButton>            ApproveButton;
	TSharedPtr<SButton>            RejectButton;
	TSharedPtr<SButton>            EditButton;
};
