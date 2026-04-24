// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#include "Session/VesselSessionConfig.h"
#include "Session/VesselSessionLog.h"
#include "Session/VesselSessionTypes.h"

namespace VesselSessionLogTestDetail
{
	static FString MakeUniqueId()
	{
		return FString::Printf(TEXT("test-session-%s"),
			*FDateTime::UtcNow().ToString(TEXT("%Y%m%d%H%M%S%s")));
	}

	static void DeleteIfExists(const FString& Path)
	{
		if (!Path.IsEmpty())
		{
			IFileManager::Get().Delete(*Path, /*RequireExists*/false, /*EvenReadOnly*/true);
		}
	}
}

/**
 * Basic lifecycle: Open → append → close; file exists; file has one line per record.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSessionLogBasic,
	"Vessel.Session.Log.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSessionLogBasic::RunTest(const FString& /*Parameters*/)
{
	const FString Id = VesselSessionLogTestDetail::MakeUniqueId();

	FVesselSessionLog Log;
	const bool bOpened = Log.Open(Id);
	TestTrue(TEXT("Log opens"), bOpened);
	TestTrue(TEXT("IsOpen true after Open"), Log.IsOpen());
	TestEqual(TEXT("GetSessionId round-trip"), Log.GetSessionId(), Id);

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("detail"), TEXT("hello"));
	Log.AppendRecord(TEXT("UnitTestEntry"), Payload);

	TSharedRef<FJsonObject> P2 = MakeShared<FJsonObject>();
	P2->SetNumberField(TEXT("count"), 42);
	Log.AppendRecord(TEXT("Counter"), P2);

	const FString FilePath = Log.GetFilePath();
	Log.Close();
	TestFalse(TEXT("IsOpen false after Close"), Log.IsOpen());

	// Verify file exists and has two non-empty lines.
	FString Contents;
	const bool bRead = FFileHelper::LoadFileToString(Contents, *FilePath);
	TestTrue(TEXT("File is readable after close"), bRead);

	TArray<FString> Lines;
	Contents.ParseIntoArrayLines(Lines, /*CullEmpty=*/true);
	TestEqual(TEXT("Exactly two JSONL lines"), Lines.Num(), 2);
	if (Lines.Num() == 2)
	{
		TestTrue(TEXT("Line 1 has type tag"),    Lines[0].Contains(TEXT("\"type\":\"UnitTestEntry\"")));
		TestTrue(TEXT("Line 1 has payload"),      Lines[0].Contains(TEXT("\"detail\":\"hello\"")));
		TestTrue(TEXT("Line 1 has session id"),   Lines[0].Contains(Id));
		TestTrue(TEXT("Line 1 has timestamp"),    Lines[0].Contains(TEXT("\"ts\":\"")));
		TestTrue(TEXT("Line 2 has numeric payload"), Lines[1].Contains(TEXT("\"count\":42")));
	}

	VesselSessionLogTestDetail::DeleteIfExists(FilePath);
	return true;
}

/**
 * Open is idempotent — calling twice closes the prior handle and opens fresh.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSessionLogReopen,
	"Vessel.Session.Log.Reopen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSessionLogReopen::RunTest(const FString& /*Parameters*/)
{
	const FString IdA = VesselSessionLogTestDetail::MakeUniqueId() + TEXT("-a");
	const FString IdB = VesselSessionLogTestDetail::MakeUniqueId() + TEXT("-b");

	FVesselSessionLog Log;
	Log.Open(IdA);
	const FString PathA = Log.GetFilePath();
	TestTrue(TEXT("Opened first file"), Log.IsOpen());

	Log.Open(IdB);
	const FString PathB = Log.GetFilePath();
	TestTrue(TEXT("Still open after reopening"), Log.IsOpen());
	TestNotEqual(TEXT("File paths differ"), PathA, PathB);

	Log.Close();
	VesselSessionLogTestDetail::DeleteIfExists(PathA);
	VesselSessionLogTestDetail::DeleteIfExists(PathB);
	return true;
}

/**
 * Config helpers: GenerateSessionId produces uniquely-increasing ids, default
 * config has sensible defaults.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSessionConfigDefault,
	"Vessel.Session.Config.Default",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSessionConfigDefault::RunTest(const FString& /*Parameters*/)
{
	const FString Id1 = GenerateSessionId();
	const FString Id2 = GenerateSessionId();
	TestTrue(TEXT("Session id starts with vs-"), Id1.StartsWith(TEXT("vs-")));
	TestNotEqual(TEXT("Consecutive ids differ"), Id1, Id2);

	const FVesselSessionConfig Config = MakeDefaultSessionConfig(FString());
	TestTrue(TEXT("Default config has session id"),       !Config.SessionId.IsEmpty());
	TestTrue(TEXT("Default agent template has name"),     !Config.AgentTemplate.Name.IsEmpty());
	TestTrue(TEXT("Default agent has system prompt"),     !Config.AgentTemplate.SystemPrompt.IsEmpty());
	TestTrue(TEXT("Default agent has judge rubric"),      !Config.AgentTemplate.JudgeRubric.IsEmpty());
	TestTrue(TEXT("Default planner model set"),           !Config.PlannerModel.IsEmpty());
	TestTrue(TEXT("Default judge model set"),             !Config.JudgeModel.IsEmpty());
	TestTrue(TEXT("Budget MaxSteps > 0"),                 Config.Budget.MaxSteps > 0);
	TestTrue(TEXT("Budget MaxCostUsd > 0"),               Config.Budget.MaxCostUsd > 0.0);
	TestTrue(TEXT("Budget MaxWallTimeSec > 0"),           Config.Budget.MaxWallTimeSec > 0);
	return true;
}

/**
 * Types: string conversions are stable (session state / outcome / judge decision).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselSessionTypesStrings,
	"Vessel.Session.Types.Strings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselSessionTypesStrings::RunTest(const FString& /*Parameters*/)
{
	TestEqual(TEXT("State Planning"),
		FString(SessionStateToString(EVesselSessionState::Planning)), FString(TEXT("Planning")));
	TestEqual(TEXT("State JudgeReview"),
		FString(SessionStateToString(EVesselSessionState::JudgeReview)), FString(TEXT("JudgeReview")));
	TestEqual(TEXT("Outcome Done"),
		FString(SessionOutcomeKindToString(EVesselSessionOutcomeKind::Done)), FString(TEXT("Done")));
	TestEqual(TEXT("Outcome AbortedOnEditorClose"),
		FString(SessionOutcomeKindToString(EVesselSessionOutcomeKind::AbortedOnEditorClose)),
		FString(TEXT("AbortedOnEditorClose")));
	TestEqual(TEXT("Judge Approve"),
		FString(JudgeDecisionToString(EVesselJudgeDecision::Approve)), FString(TEXT("Approve")));
	TestEqual(TEXT("Judge Revise"),
		FString(JudgeDecisionToString(EVesselJudgeDecision::Revise)), FString(TEXT("Revise")));
	return true;
}
