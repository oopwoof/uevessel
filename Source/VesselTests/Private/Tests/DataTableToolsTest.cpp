// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Misc/AutomationTest.h"

#include "Fixtures/VesselTestToolFixture.h"
#include "Tools/VesselDataTableTools.h"

#include "Engine/DataTable.h"
#include "UObject/StrongObjectPtr.h"

namespace VesselDataTableTestDetail
{
	/**
	 * Construct a transient in-memory DataTable with three rows of FVesselTestRow.
	 * Returned via TStrongObjectPtr so the test keeps the UDataTable alive across
	 * any GC pass that might fire during latent/async variants of this test.
	 */
	static TStrongObjectPtr<UDataTable> MakeInMemoryTable()
	{
		TStrongObjectPtr<UDataTable> Table(
			NewObject<UDataTable>(GetTransientPackage(), UDataTable::StaticClass()));
		Table->RowStruct = FVesselTestRow::StaticStruct();

		FVesselTestRow Alpha; Alpha.Title = TEXT("Alpha"); Alpha.Age = 20; Alpha.bActive = true;
		FVesselTestRow Beta;  Beta.Title  = TEXT("Beta");  Beta.Age  = 30; Beta.bActive  = false;
		FVesselTestRow Gamma; Gamma.Title = TEXT("Gamma"); Gamma.Age = 45; Gamma.bActive = true;

		Table->AddRow(FName(TEXT("Row_Alpha")), Alpha);
		Table->AddRow(FName(TEXT("Row_Beta")),  Beta);
		Table->AddRow(FName(TEXT("Row_Gamma")), Gamma);
		return Table;
	}
}

/**
 * ReadRowsJson with empty RowNames returns every row.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselDataTableReadAll,
	"Vessel.Tools.DataTable.ReadAll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselDataTableReadAll::RunTest(const FString& /*Parameters*/)
{
	TStrongObjectPtr<UDataTable> Table = VesselDataTableTestDetail::MakeInMemoryTable();
	TestNotNull(TEXT("In-memory table created"), Table.Get());
	if (!Table.IsValid()) { return false; }

	const FString Json = UVesselDataTableTools::ReadRowsJson(Table.Get(), TArray<FName>());
	TestTrue(TEXT("All rows: Row_Alpha present"), Json.Contains(TEXT("Row_Alpha")));
	TestTrue(TEXT("All rows: Row_Beta present"),  Json.Contains(TEXT("Row_Beta")));
	TestTrue(TEXT("All rows: Row_Gamma present"), Json.Contains(TEXT("Row_Gamma")));
	TestTrue(TEXT("Row fields serialized (Title)"), Json.Contains(TEXT("\"Alpha\"")));
	TestTrue(TEXT("Row fields serialized (Age 30)"), Json.Contains(TEXT("30")));
	TestTrue(TEXT("Row fields serialized (bActive true)"), Json.Contains(TEXT("true")));
	return true;
}

/**
 * ReadRowsJson with explicit RowNames returns only those rows.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselDataTableReadSelected,
	"Vessel.Tools.DataTable.ReadSelected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselDataTableReadSelected::RunTest(const FString& /*Parameters*/)
{
	TStrongObjectPtr<UDataTable> Table = VesselDataTableTestDetail::MakeInMemoryTable();
	if (!Table.IsValid()) { return false; }

	TArray<FName> Selected;
	Selected.Add(FName(TEXT("Row_Alpha")));
	Selected.Add(FName(TEXT("Row_Gamma")));

	const FString Json = UVesselDataTableTools::ReadRowsJson(Table.Get(), Selected);
	TestTrue(TEXT("Selected: Alpha present"), Json.Contains(TEXT("Row_Alpha")));
	TestTrue(TEXT("Selected: Gamma present"), Json.Contains(TEXT("Row_Gamma")));
	TestFalse(TEXT("Selected: Beta absent"),   Json.Contains(TEXT("Row_Beta")));
	return true;
}

/**
 * ReadRowsJson tolerates a null table without crashing.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselDataTableNullTable,
	"Vessel.Tools.DataTable.NullTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselDataTableNullTable::RunTest(const FString& /*Parameters*/)
{
	const FString Json = UVesselDataTableTools::ReadRowsJson(nullptr, TArray<FName>());
	TestEqual(TEXT("Null table → empty JSON object"), Json, FString(TEXT("{}")));
	return true;
}

/**
 * Asking for a nonexistent row just omits it, does not error.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselDataTableUnknownRow,
	"Vessel.Tools.DataTable.UnknownRow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselDataTableUnknownRow::RunTest(const FString& /*Parameters*/)
{
	TStrongObjectPtr<UDataTable> Table = VesselDataTableTestDetail::MakeInMemoryTable();
	if (!Table.IsValid()) { return false; }

	TArray<FName> Missing;
	Missing.Add(FName(TEXT("_NoSuchRow")));
	Missing.Add(FName(TEXT("Row_Beta")));

	const FString Json = UVesselDataTableTools::ReadRowsJson(Table.Get(), Missing);
	TestFalse(TEXT("Unknown row omitted"), Json.Contains(TEXT("_NoSuchRow")));
	TestTrue(TEXT("Known row still present"), Json.Contains(TEXT("Row_Beta")));
	return true;
}

/**
 * WriteRowJson inserts a new row and it appears in subsequent reads.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselDataTableWriteInsert,
	"Vessel.Tools.DataTable.WriteInsert",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselDataTableWriteInsert::RunTest(const FString& /*Parameters*/)
{
	TStrongObjectPtr<UDataTable> Table = VesselDataTableTestDetail::MakeInMemoryTable();
	if (!Table.IsValid()) { return false; }

	const FString NewRowJson = TEXT("{\"Title\":\"Delta\",\"Age\":99,\"bActive\":true}");
	const bool bOk = UVesselDataTableTools::WriteRowJson(Table.Get(), FName(TEXT("Row_Delta")), NewRowJson);
	TestTrue(TEXT("WriteRowJson reports success"), bOk);

	const FString Json = UVesselDataTableTools::ReadRowsJson(Table.Get(), TArray<FName>());
	TestTrue(TEXT("Row_Delta visible in read-back"), Json.Contains(TEXT("Row_Delta")));
	TestTrue(TEXT("Delta Title persisted"),         Json.Contains(TEXT("\"Delta\"")));
	TestTrue(TEXT("Delta Age=99 persisted"),        Json.Contains(TEXT("99")));
	return true;
}

/**
 * WriteRowJson on an existing row replaces its contents.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselDataTableWriteReplace,
	"Vessel.Tools.DataTable.WriteReplace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselDataTableWriteReplace::RunTest(const FString& /*Parameters*/)
{
	TStrongObjectPtr<UDataTable> Table = VesselDataTableTestDetail::MakeInMemoryTable();
	if (!Table.IsValid()) { return false; }

	const FString UpdatedJson = TEXT("{\"Title\":\"AlphaPrime\",\"Age\":21,\"bActive\":false}");
	const bool bOk = UVesselDataTableTools::WriteRowJson(Table.Get(), FName(TEXT("Row_Alpha")), UpdatedJson);
	TestTrue(TEXT("WriteRowJson replace succeeds"), bOk);

	TArray<FName> Just;
	Just.Add(FName(TEXT("Row_Alpha")));
	const FString Json = UVesselDataTableTools::ReadRowsJson(Table.Get(), Just);
	TestTrue(TEXT("Row_Alpha still present"),          Json.Contains(TEXT("Row_Alpha")));
	TestTrue(TEXT("Replaced Title visible"),           Json.Contains(TEXT("\"AlphaPrime\"")));
	TestFalse(TEXT("Old Title (\"Alpha\") removed"),   Json.Contains(TEXT("\"Alpha\"") TEXT(",\"Age\":20")));
	return true;
}

/**
 * Invalid JSON / missing fields → WriteRowJson returns false, table untouched.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVesselDataTableWriteRejectsBadJson,
	"Vessel.Tools.DataTable.WriteRejectsBadJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FVesselDataTableWriteRejectsBadJson::RunTest(const FString& /*Parameters*/)
{
	TStrongObjectPtr<UDataTable> Table = VesselDataTableTestDetail::MakeInMemoryTable();
	if (!Table.IsValid()) { return false; }

	const bool bOk = UVesselDataTableTools::WriteRowJson(
		Table.Get(), FName(TEXT("Row_Rubbish")), TEXT("not a json"));
	TestFalse(TEXT("Rejects non-JSON input"), bOk);

	const FString Json = UVesselDataTableTools::ReadRowsJson(Table.Get(), TArray<FName>());
	TestFalse(TEXT("Bad row not inserted"), Json.Contains(TEXT("Row_Rubbish")));
	return true;
}
