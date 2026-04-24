// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Registry/VesselToolRegistry.h"
#include "Registry/VesselReflectionScanner.h"
#include "VesselLog.h"

namespace
{
	FString EscapeJsonStr(const FString& In)
	{
		FString Out;
		Out.Reserve(In.Len() + 4);
		for (const TCHAR C : In)
		{
			switch (C)
			{
				case TEXT('"'):  Out += TEXT("\\\""); break;
				case TEXT('\\'): Out += TEXT("\\\\"); break;
				case TEXT('\n'): Out += TEXT("\\n"); break;
				case TEXT('\r'): Out += TEXT("\\r"); break;
				case TEXT('\t'): Out += TEXT("\\t"); break;
				default:
					if (C < 0x20)
					{
						Out += FString::Printf(TEXT("\\u%04x"), static_cast<int32>(C));
					}
					else
					{
						Out.AppendChar(C);
					}
					break;
			}
		}
		return Out;
	}

	FString RenderSchemaJson(const FVesselToolSchema& S)
	{
		FString Params;
		FString Required;
		bool bFirst = true;
		for (const FVesselParameterSchema& P : S.Parameters)
		{
			if (!bFirst) Params += TEXT(",");
			bFirst = false;
			Params += FString::Printf(TEXT("\"%s\":%s"),
				*EscapeJsonStr(P.Name.ToString()), *P.TypeJson);

			if (P.bRequired)
			{
				if (!Required.IsEmpty()) Required += TEXT(",");
				Required += FString::Printf(TEXT("\"%s\""), *EscapeJsonStr(P.Name.ToString()));
			}
		}
		const FString Tags = [&]()
		{
			FString T;
			for (int32 i = 0; i < S.Tags.Num(); ++i)
			{
				if (i > 0) T += TEXT(",");
				T += FString::Printf(TEXT("\"%s\""), *EscapeJsonStr(S.Tags[i]));
			}
			return T;
		}();

		return FString::Printf(
			TEXT("{")
			TEXT("\"name\":\"%s\",")
			TEXT("\"category\":\"%s\",")
			TEXT("\"description\":\"%s\",")
			TEXT("\"requires_approval\":%s,")
			TEXT("\"irreversible\":%s,")
			TEXT("\"batch_eligible\":%s,")
			TEXT("\"min_vessel_version\":\"%s\",")
			TEXT("\"tags\":[%s],")
			TEXT("\"parameters\":{\"type\":\"object\",\"properties\":{%s},\"required\":[%s]},")
			TEXT("\"returns\":%s,")
			TEXT("\"source\":{\"class\":\"%s\",\"function\":\"%s\",\"module\":\"%s\"}")
			TEXT("}"),
			*EscapeJsonStr(S.Name.ToString()),
			*EscapeJsonStr(S.Category),
			*EscapeJsonStr(S.Description),
			S.bRequiresApproval ? TEXT("true") : TEXT("false"),
			S.bIrreversibleHint ? TEXT("true") : TEXT("false"),
			S.bBatchEligible    ? TEXT("true") : TEXT("false"),
			*EscapeJsonStr(S.MinVesselVersion),
			*Tags,
			*Params,
			*Required,
			S.ReturnTypeJson.IsEmpty() ? TEXT("null") : *S.ReturnTypeJson,
			*EscapeJsonStr(S.SourceClassName),
			*EscapeJsonStr(S.SourceFunctionName),
			*EscapeJsonStr(S.SourceModuleName));
	}
}

FVesselToolRegistry& FVesselToolRegistry::Get()
{
	static FVesselToolRegistry Instance;
	return Instance;
}

void FVesselToolRegistry::ScanAll()
{
	const TArray<FVesselToolSchema> Scanned = FVesselReflectionScanner::BuildToolSchemas();

	FRWScopeLock WLock(Lock, SLT_Write);
	Schemas.Empty(Scanned.Num());
	for (const FVesselToolSchema& S : Scanned)
	{
		Schemas.Add(S.Name, S);
	}
	UE_LOG(LogVesselRegistry, Log, TEXT("Tool registry populated: %d tools."), Schemas.Num());
}

void FVesselToolRegistry::ClearAll()
{
	FRWScopeLock WLock(Lock, SLT_Write);
	Schemas.Empty();
}

void FVesselToolRegistry::InjectSchemaForTest(const FVesselToolSchema& Schema)
{
	FRWScopeLock WLock(Lock, SLT_Write);
	Schemas.Add(Schema.Name, Schema);
}

int32 FVesselToolRegistry::Num() const
{
	FRWScopeLock RLock(const_cast<FRWLock&>(Lock), SLT_ReadOnly);
	return Schemas.Num();
}

const FVesselToolSchema* FVesselToolRegistry::FindSchema(FName ToolName) const
{
	FRWScopeLock RLock(const_cast<FRWLock&>(Lock), SLT_ReadOnly);
	return Schemas.Find(ToolName);
}

TArray<FName> FVesselToolRegistry::ListToolNames() const
{
	FRWScopeLock RLock(const_cast<FRWLock&>(Lock), SLT_ReadOnly);
	TArray<FName> Names;
	Schemas.GetKeys(Names);
	return Names;
}

TArray<FVesselToolSchema> FVesselToolRegistry::GetAllSchemas() const
{
	FRWScopeLock RLock(const_cast<FRWLock&>(Lock), SLT_ReadOnly);
	TArray<FVesselToolSchema> Out;
	Schemas.GenerateValueArray(Out);
	return Out;
}

FString FVesselToolRegistry::ToJsonString() const
{
	FRWScopeLock RLock(const_cast<FRWLock&>(Lock), SLT_ReadOnly);
	FString Body;
	bool bFirst = true;
	for (const TPair<FName, FVesselToolSchema>& Pair : Schemas)
	{
		if (!bFirst) Body += TEXT(",");
		bFirst = false;
		Body += RenderSchemaJson(Pair.Value);
	}
	return FString::Printf(TEXT("[%s]"), *Body);
}
