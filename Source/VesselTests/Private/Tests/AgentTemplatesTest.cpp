// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Misc/AutomationTest.h"
#include "Session/VesselAgentTemplates.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselAgentTemplatesDesignerShape,
	"Vessel.Session.AgentTemplates.DesignerShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselAgentTemplatesDesignerShape::RunTest(const FString& /*Parameters*/)
{
	const FVesselAgentTemplate T = FVesselAgentTemplates::MakeDesignerAssistant();
	TestEqual(TEXT("Name is designer-assistant"), T.Name, FString(TEXT("designer-assistant")));
	TestTrue(TEXT("System prompt non-empty"),     !T.SystemPrompt.IsEmpty());
	TestTrue(TEXT("Judge rubric non-empty"),      !T.JudgeRubric.IsEmpty());
	TestTrue(TEXT("Allowed includes DataTable"),  T.AllowedCategories.Contains(TEXT("DataTable")));
	TestTrue(TEXT("Allowed includes Meta"),       T.AllowedCategories.Contains(TEXT("Meta")));
	TestFalse(TEXT("Write not granted by default"),
		T.AllowedCategories.Contains(TEXT("DataTable/Write")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselAgentTemplatesLookup,
	"Vessel.Session.AgentTemplates.Lookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselAgentTemplatesLookup::RunTest(const FString& /*Parameters*/)
{
	TestEqual(TEXT("Designer lookup by name"),
		FVesselAgentTemplates::FindByName(TEXT("designer-assistant")).Name,
		FString(TEXT("designer-assistant")));

	// Unknown name falls back to the minimal built-in.
	const FVesselAgentTemplate Unknown =
		FVesselAgentTemplates::FindByName(TEXT("no-such-agent-xyz"));
	TestEqual(TEXT("Unknown name falls back to vessel-default"),
		Unknown.Name, FString(TEXT("vessel-default")));

	// Listing returns the known names in stable order.
	const TArray<FString> All = FVesselAgentTemplates::ListNames();
	TestTrue(TEXT("ListNames contains designer-assistant"),
		All.Contains(TEXT("designer-assistant")));
	return true;
}
