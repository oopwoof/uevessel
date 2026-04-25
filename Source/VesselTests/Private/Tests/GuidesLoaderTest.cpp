// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#include "Session/VesselGuidesLoader.h"

namespace VesselGuidesTestDetail
{
	/**
	 * Snapshot the project's AGENTS.md (if any), let the test write its own,
	 * restore on cleanup. Without this, tests pollute the repo's real AGENTS.md.
	 */
	struct FAgentsMdSandbox
	{
		FString Path;
		FString Backup;
		bool    bExisted = false;

		FAgentsMdSandbox()
		{
			Path = FVesselGuidesLoader::GetAgentsMdPath();
			bExisted = FFileHelper::LoadFileToString(Backup, *Path);
		}
		~FAgentsMdSandbox()
		{
			if (bExisted)
			{
				FFileHelper::SaveStringToFile(Backup, *Path);
			}
			else
			{
				IFileManager::Get().Delete(*Path, /*RequireExists*/ false, /*EvenReadOnly*/ true);
			}
		}

		void WriteContent(const FString& Content)
		{
			FFileHelper::SaveStringToFile(Content, *Path);
		}
	};

	static FString BuildSampleAgentsMd()
	{
		// Mirror the layout FVesselRejectionSink emits.
		FString S;
		S += TEXT("# AGENTS.md\n\n");
		S += TEXT("Designers should never overwrite localized text without confirming with the LOC team.\n");
		S += TEXT("All DataTable schema changes must go through the schema review channel.\n\n");
		S += TEXT("## Known Rejections\n");
		S += TEXT("<!-- auto-managed by Vessel HITL Gate -->\n\n");

		// Two rejection entries
		S += TEXT("### 2026-04-20T10:00:00.000Z · tool=WriteDataTableRow · target=/Game/DT_NPC.DT_NPC\n");
		S += TEXT("**reason**: Tried to overwrite production NPC stats without backup branch.\n");
		S += TEXT("**session**: vs-2026-04-20-0001, step 2\n");
		S += TEXT("**rejecter**: slate-panel\n\n");

		S += TEXT("### 2026-04-22T15:30:00.000Z · tool=WriteDataTableRow · target=/Game/DT_Items.DT_Items\n");
		S += TEXT("**reason**: New row used inventory_class field which does not exist in the row struct.\n");
		S += TEXT("**session**: vs-2026-04-22-0007, step 1\n");
		S += TEXT("**rejecter**: slate-panel\n");
		return S;
	}
}

/* ─── Test 1: Split preamble and rejections section ─────────────────── */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselGuidesLoaderSplit,
	"Vessel.Guides.Loader.SplitPreambleAndRejections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselGuidesLoaderSplit::RunTest(const FString& /*Parameters*/)
{
	const FString Full = VesselGuidesTestDetail::BuildSampleAgentsMd();

	FString Preamble, Rejections;
	FVesselGuidesLoader::SplitPreambleAndRejections(Full, Preamble, Rejections);

	TestTrue(TEXT("Preamble contains user guidance"),
		Preamble.Contains(TEXT("Designers should never overwrite localized text")));
	TestFalse(TEXT("Preamble does NOT contain rejections header"),
		Preamble.Contains(TEXT("## Known Rejections")));
	TestTrue(TEXT("Rejections section starts with header"),
		Rejections.StartsWith(TEXT("## Known Rejections")));
	TestTrue(TEXT("Rejections section contains both entries"),
		Rejections.Contains(TEXT("inventory_class field")));

	// File with no rejections section: the whole file is preamble.
	FString PreambleOnly = TEXT("# AGENTS.md\n\nNo rejections here yet.\n");
	FString P2, R2;
	FVesselGuidesLoader::SplitPreambleAndRejections(PreambleOnly, P2, R2);
	TestEqual(TEXT("Preamble-only: rejections empty"), R2, FString());
	TestTrue(TEXT("Preamble-only: preamble preserved"),
		P2.Contains(TEXT("No rejections here yet")));
	return true;
}

/* ─── Test 2: ParseRejections extracts tool / target / reason ───────── */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselGuidesLoaderParse,
	"Vessel.Guides.Loader.ParseRejections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselGuidesLoaderParse::RunTest(const FString& /*Parameters*/)
{
	const FString Full = VesselGuidesTestDetail::BuildSampleAgentsMd();
	FString Preamble, RejectionsSection;
	FVesselGuidesLoader::SplitPreambleAndRejections(Full, Preamble, RejectionsSection);

	const TArray<FVesselGuidesLoader::FRejectionEntry> Entries =
		FVesselGuidesLoader::ParseRejections(RejectionsSection);

	TestEqual(TEXT("Two entries parsed"), Entries.Num(), 2);
	if (Entries.Num() < 2) return false;

	TestEqual(TEXT("[0] tool"), Entries[0].Tool,
		FString(TEXT("WriteDataTableRow")));
	TestEqual(TEXT("[0] target"), Entries[0].Target,
		FString(TEXT("/Game/DT_NPC.DT_NPC")));
	TestTrue(TEXT("[0] reason has 'production NPC'"),
		Entries[0].Reason.Contains(TEXT("production NPC")));

	TestEqual(TEXT("[1] tool"), Entries[1].Tool,
		FString(TEXT("WriteDataTableRow")));
	TestEqual(TEXT("[1] target"), Entries[1].Target,
		FString(TEXT("/Game/DT_Items.DT_Items")));
	TestTrue(TEXT("[1] reason has 'inventory_class'"),
		Entries[1].Reason.Contains(TEXT("inventory_class")));
	return true;
}

/* ─── Test 3: ParseRejections caps at MaxEntries ────────────────────── */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselGuidesLoaderParseCap,
	"Vessel.Guides.Loader.ParseRejectionsRespectsCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselGuidesLoaderParseCap::RunTest(const FString& /*Parameters*/)
{
	const FString Full = VesselGuidesTestDetail::BuildSampleAgentsMd();
	FString Preamble, RejectionsSection;
	FVesselGuidesLoader::SplitPreambleAndRejections(Full, Preamble, RejectionsSection);

	const TArray<FVesselGuidesLoader::FRejectionEntry> Capped =
		FVesselGuidesLoader::ParseRejections(RejectionsSection, /*MaxEntries=*/ 1);
	TestEqual(TEXT("Cap at 1 entry"), Capped.Num(), 1);
	return true;
}

/* ─── Test 4: BuildProjectGuidesBlock emits LLM-readable text ───────── */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselGuidesLoaderBuildBlock,
	"Vessel.Guides.Loader.BuildProjectGuidesBlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselGuidesLoaderBuildBlock::RunTest(const FString& /*Parameters*/)
{
	VesselGuidesTestDetail::FAgentsMdSandbox Sandbox;
	Sandbox.WriteContent(VesselGuidesTestDetail::BuildSampleAgentsMd());

	const FString Block = FVesselGuidesLoader::BuildProjectGuidesBlock();
	TestFalse(TEXT("Block is non-empty"), Block.IsEmpty());

	TestTrue(TEXT("Has 'Project guides' header"),
		Block.Contains(TEXT("## Project guides")));
	TestTrue(TEXT("Has 'Past rejections' header"),
		Block.Contains(TEXT("## Past rejections")));

	// Preamble content should land in the block.
	TestTrue(TEXT("Block carries user-edited preamble"),
		Block.Contains(TEXT("never overwrite localized text"))
		|| Block.Contains(TEXT("schema review channel")));

	// Rejection entries should be compacted into "tool=X on Y: reason".
	TestTrue(TEXT("Block has compacted entry 1"),
		Block.Contains(TEXT("tool=WriteDataTableRow"))
		&& Block.Contains(TEXT("/Game/DT_NPC.DT_NPC")));
	TestTrue(TEXT("Block has compacted entry 2 (inventory_class)"),
		Block.Contains(TEXT("inventory_class")));

	// Block should NOT carry the audit-trail noise (timestamps, session id,
	// rejecter) — those are useful in AGENTS.md for humans, useless to LLM.
	TestFalse(TEXT("Block does not include timestamps"),
		Block.Contains(TEXT("2026-04-20T10:00")));
	TestFalse(TEXT("Block does not include session ids"),
		Block.Contains(TEXT("vs-2026-04-20-0001")));
	return true;
}

/* ─── Test 5: BuildProjectGuidesBlock empty when AGENTS.md missing ──── */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselGuidesLoaderEmpty,
	"Vessel.Guides.Loader.BuildBlockMissingFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselGuidesLoaderEmpty::RunTest(const FString& /*Parameters*/)
{
	VesselGuidesTestDetail::FAgentsMdSandbox Sandbox;
	// Sandbox dtor will restore — for now delete.
	IFileManager::Get().Delete(*Sandbox.Path, /*RequireExists*/false, /*EvenReadOnly*/true);

	const FString Block = FVesselGuidesLoader::BuildProjectGuidesBlock();
	TestEqual(TEXT("No file → empty block"), Block, FString());
	return true;
}

/* ─── Test 6: Most-recent-N selection when many entries exist ───────── */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselGuidesLoaderRecencyCap,
	"Vessel.Guides.Loader.MostRecentNCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselGuidesLoaderRecencyCap::RunTest(const FString& /*Parameters*/)
{
	// Synthesize 5 entries; ask for last 2.
	FString Full;
	Full += TEXT("# AGENTS.md\n\n");
	Full += TEXT("## Known Rejections\n");
	for (int32 i = 1; i <= 5; ++i)
	{
		Full += FString::Printf(
			TEXT("\n### 2026-04-20T10:00:0%d.000Z · tool=Tool%d · target=/Game/X%d\n"),
			i, i, i);
		Full += FString::Printf(TEXT("**reason**: reason%d\n"), i);
	}

	VesselGuidesTestDetail::FAgentsMdSandbox Sandbox;
	Sandbox.WriteContent(Full);

	const FString Block = FVesselGuidesLoader::BuildProjectGuidesBlock(
		/*MaxRejections=*/ 2);

	// Should contain the LAST two (Tool4, Tool5) and NOT contain Tool1/2/3.
	TestTrue(TEXT("Block contains Tool4"),  Block.Contains(TEXT("tool=Tool4")));
	TestTrue(TEXT("Block contains Tool5"),  Block.Contains(TEXT("tool=Tool5")));
	TestFalse(TEXT("Block does not contain Tool1"), Block.Contains(TEXT("tool=Tool1")));
	TestFalse(TEXT("Block does not contain Tool2"), Block.Contains(TEXT("tool=Tool2")));
	TestFalse(TEXT("Block does not contain Tool3"), Block.Contains(TEXT("tool=Tool3")));
	return true;
}

/* ─── Test 7: Multi-line reason captures continuation lines ─────────── */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselGuidesLoaderMultiLineReason,
	"Vessel.Guides.Loader.MultiLineReasonAccumulates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselGuidesLoaderMultiLineReason::RunTest(const FString& /*Parameters*/)
{
	// Reason that spans multiple lines, terminated by **session**.
	FString Section;
	Section += TEXT("## Known Rejections\n\n");
	Section += TEXT("### 2026-04-25T00:00:00.000Z · tool=Foo · target=/Game/X\n");
	Section += TEXT("**reason**: First line of explanation.\n");
	Section += TEXT("Second line continues the reasoning.\n");
	Section += TEXT("Third line ends the paragraph.\n");
	Section += TEXT("**session**: vs-2026-04-25-0001, step 1\n");
	Section += TEXT("**rejecter**: slate-panel\n");

	const TArray<FVesselGuidesLoader::FRejectionEntry> Entries =
		FVesselGuidesLoader::ParseRejections(Section);

	TestEqual(TEXT("One entry parsed"), Entries.Num(), 1);
	if (Entries.Num() < 1) return false;

	TestTrue(TEXT("Reason has line 1"),
		Entries[0].Reason.Contains(TEXT("First line of explanation")));
	TestTrue(TEXT("Reason has line 2 (continuation)"),
		Entries[0].Reason.Contains(TEXT("Second line continues")));
	TestTrue(TEXT("Reason has line 3 (continuation)"),
		Entries[0].Reason.Contains(TEXT("Third line ends")));
	TestFalse(TEXT("Reason does not bleed into **session**"),
		Entries[0].Reason.Contains(TEXT("vs-2026-04-25-0001")));
	return true;
}

/* ─── Test 8: Multi-line reason still terminated by next ### header ─── */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselGuidesLoaderMultiLineTerminatesAtNextEntry,
	"Vessel.Guides.Loader.MultiLineReasonTerminatesAtNextEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselGuidesLoaderMultiLineTerminatesAtNextEntry::RunTest(const FString& /*Parameters*/)
{
	// Worst-case: rejection sink crashed mid-write, no **session** terminator.
	// The next "### " entry header must still cleanly terminate the reason.
	FString Section;
	Section += TEXT("## Known Rejections\n\n");
	Section += TEXT("### 2026-04-25T00:00:00.000Z · tool=Foo · target=/Game/X\n");
	Section += TEXT("**reason**: First entry's reason.\n");
	Section += TEXT("Continuation line.\n");
	Section += TEXT("\n");
	Section += TEXT("### 2026-04-26T00:00:00.000Z · tool=Bar · target=/Game/Y\n");
	Section += TEXT("**reason**: Second entry's reason.\n");

	const TArray<FVesselGuidesLoader::FRejectionEntry> Entries =
		FVesselGuidesLoader::ParseRejections(Section);

	TestEqual(TEXT("Two entries parsed"), Entries.Num(), 2);
	if (Entries.Num() < 2) return false;

	TestTrue(TEXT("[0] reason contains continuation"),
		Entries[0].Reason.Contains(TEXT("Continuation line")));
	TestFalse(TEXT("[0] reason does not bleed into [1]"),
		Entries[0].Reason.Contains(TEXT("Second entry's reason")));
	TestEqual(TEXT("[1] tool"), Entries[1].Tool, FString(TEXT("Bar")));
	return true;
}

/* ─── Test 9: Planner request actually injects the guides block ─────── */

#include "Registry/VesselToolSchema.h"
#include "Session/VesselAgentTemplates.h"
#include "Session/VesselPlannerPrompts.h"
#include "Session/VesselSessionConfig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselGuidesLoaderPlannerInject,
	"Vessel.Guides.Loader.PlannerInjectsBlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselGuidesLoaderPlannerInject::RunTest(const FString& /*Parameters*/)
{
	VesselGuidesTestDetail::FAgentsMdSandbox Sandbox;
	Sandbox.WriteContent(VesselGuidesTestDetail::BuildSampleAgentsMd());

	FVesselSessionConfig Cfg = MakeDefaultSessionConfig(TEXT("guides-inject-test"));
	Cfg.AgentTemplate = FVesselAgentTemplates::MakeDesignerAssistant();

	FVesselToolSchema FakeTool;
	FakeTool.Name        = FName(TEXT("FakeTool"));
	FakeTool.Category    = TEXT("DataTable");
	FakeTool.Description = TEXT("placeholder");

	const FLlmRequest Req = FVesselPlannerPrompts::BuildPlanningRequest(
		Cfg, TEXT("anything"), { FakeTool }, FString());

	TestTrue(TEXT("Request has at least system + user message"),
		Req.Messages.Num() >= 2);
	const FLlmMessage& Sys = Req.Messages[0];

	TestTrue(TEXT("System prompt carries 'Past rejections' block"),
		Sys.Content.Contains(TEXT("Past rejections")));
	TestTrue(TEXT("System prompt carries the inventory_class rejection"),
		Sys.Content.Contains(TEXT("inventory_class")));
	TestTrue(TEXT("System prompt carries the production-NPC rejection"),
		Sys.Content.Contains(TEXT("production NPC")));
	return true;
}
