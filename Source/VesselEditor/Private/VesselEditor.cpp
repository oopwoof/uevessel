// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "VesselEditor.h"
#include "VesselLog.h"
#include "Registry/VesselToolRegistry.h"
#include "Widgets/SVesselChatPanel.h"
#include "Widgets/VesselTabIds.h"

#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "VesselEditorModule"

namespace VesselEditorModuleDetail
{
	/** Owner tag for all menu extensions so we can bulk-remove on shutdown. */
	static const FName VesselMenuOwner = TEXT("VesselEditorModule.Menus");

	/** Build the dock tab hosting the chat panel. */
	static TSharedRef<SDockTab> SpawnChatPanelTab(const FSpawnTabArgs& /*Args*/)
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			.Label(LOCTEXT("VesselChatTabLabel", "Vessel"))
			[
				SNew(SVesselChatPanel)
			];
	}

	/** Add a Window menu entry so users can open the panel. */
	static void RegisterWindowMenuEntry()
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window"));
		if (!Menu)
		{
			return;
		}
		FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("Vessel"));
		Section.AddMenuEntry(
			TEXT("VesselChatPanelOpen"),
			LOCTEXT("VesselWindowMenuLabel",   "Vessel Chat"),
			LOCTEXT("VesselWindowMenuTooltip", "Open the Vessel agent chat + HITL panel."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(FTabId(VesselTabIds::ChatPanel));
			})));
	}
}

void FVesselEditorModule::StartupModule()
{
	UE_LOG(LogVessel, Log, TEXT("VesselEditor module started."));

	FGlobalTabmanager::Get()
		->RegisterNomadTabSpawner(
			VesselTabIds::ChatPanel,
			FOnSpawnTab::CreateStatic(&VesselEditorModuleDetail::SpawnChatPanelTab))
		.SetDisplayName(LOCTEXT("VesselChatTabDisplayName", "Vessel"))
		.SetTooltipText(LOCTEXT("VesselChatTabTooltip",
			"Vessel — agent chat + HITL approval panel."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());

	// Deferred to first tick so the ToolMenus subsystem has finished bootstrapping.
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateStatic(
			&VesselEditorModuleDetail::RegisterWindowMenuEntry));

	// Populate the agent tool registry once all UClasses are loaded.
	// PostEngineInit fires after every plugin is initialised, which is when
	// reflection-driven scanning sees every UFUNCTION marked AgentTool. Without
	// this, the registry stays empty in production and any LLM session fails
	// with "unknown tool" — caught during v0.2 L5 (2026-04-24).
	FCoreDelegates::OnPostEngineInit.AddLambda([]()
	{
		FVesselToolRegistry::Get().ScanAll();
		UE_LOG(LogVessel, Log,
			TEXT("Vessel tool registry: %d tool(s) registered."),
			FVesselToolRegistry::Get().Num());
	});
}

void FVesselEditorModule::ShutdownModule()
{
	UE_LOG(LogVessel, Log, TEXT("VesselEditor module shutting down."));

	// Unregister the dock tab spawner so the Workspace → Tools entry goes away.
	// FGlobalTabmanager::Get() returns a TSharedRef which is always valid by
	// definition; calling IsValid on a TSharedRef is rejected in UE 5.7 onward.
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(VesselTabIds::ChatPanel);

	// Remove any menu extensions we added. Guarded because ShutdownModule can
	// run after UObject shutdown during editor quit, where UToolMenus::Get()
	// would touch destroyed globals.
	if (UObjectInitialized())
	{
		if (UToolMenus* Menus = UToolMenus::Get())
		{
			// Owner-scoped unregister covers any extensions we tagged with
			// VesselMenuOwner. The Window-menu section we added above is not
			// owner-tagged in UE 5.5 (API does not surface FToolMenuOwner
			// through FindOrAddSection), so we also explicitly drop our entry.
			Menus->UnregisterOwnerByName(VesselEditorModuleDetail::VesselMenuOwner);
			if (UToolMenu* WindowMenu = Menus->ExtendMenu(TEXT("LevelEditor.MainMenu.Window")))
			{
				WindowMenu->RemoveSection(TEXT("Vessel"));
			}
		}
	}
}

IMPLEMENT_MODULE(FVesselEditorModule, VesselEditor);

#undef LOCTEXT_NAMESPACE
