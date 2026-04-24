// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Tools/VesselDataTableTools.h"

#include "VesselLog.h"

#include "Engine/DataTable.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"

namespace VesselDataTableDetail
{
	/** Serialize a single row struct instance to a JsonObject. Best-effort — unknown types render as null. */
	static TSharedRef<FJsonObject> RowToJsonObject(UScriptStruct* RowStruct, const void* RowData)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		if (!RowStruct || !RowData)
		{
			return Obj;
		}

		for (TFieldIterator<FProperty> It(RowStruct); It; ++It)
		{
			FProperty* Prop = *It;
			const void* ValueAddr = Prop->ContainerPtrToValuePtr<void>(RowData);

			if (FStrProperty* StrProp = CastField<FStrProperty>(Prop))
			{
				Obj->SetStringField(Prop->GetName(), StrProp->GetPropertyValue(ValueAddr));
			}
			else if (FNameProperty* NameProp = CastField<FNameProperty>(Prop))
			{
				Obj->SetStringField(Prop->GetName(), NameProp->GetPropertyValue(ValueAddr).ToString());
			}
			else if (FTextProperty* TextProp = CastField<FTextProperty>(Prop))
			{
				Obj->SetStringField(Prop->GetName(), TextProp->GetPropertyValue(ValueAddr).ToString());
			}
			else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
			{
				Obj->SetBoolField(Prop->GetName(), BoolProp->GetPropertyValue(ValueAddr));
			}
			else if (FIntProperty* IntProp = CastField<FIntProperty>(Prop))
			{
				Obj->SetNumberField(Prop->GetName(), IntProp->GetPropertyValue(ValueAddr));
			}
			else if (FInt64Property* Int64Prop = CastField<FInt64Property>(Prop))
			{
				Obj->SetNumberField(Prop->GetName(), static_cast<double>(Int64Prop->GetPropertyValue(ValueAddr)));
			}
			else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
			{
				Obj->SetNumberField(Prop->GetName(), FloatProp->GetPropertyValue(ValueAddr));
			}
			else if (FDoubleProperty* DblProp = CastField<FDoubleProperty>(Prop))
			{
				Obj->SetNumberField(Prop->GetName(), DblProp->GetPropertyValue(ValueAddr));
			}
			else
			{
				// Unsupported type — mark as null so LLM knows there was a field here it can't see yet.
				Obj->SetField(Prop->GetName(), MakeShared<FJsonValueNull>());
			}
		}
		return Obj;
	}

	static FString SerializeAsCondensedJson(const TSharedRef<FJsonObject>& Root)
	{
		FString Out;
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Root, Writer);
		return Out;
	}
}

FString UVesselDataTableTools::ReadRowsJson(UDataTable* Table, const TArray<FName>& RowNames)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	if (!Table)
	{
		// Empty object — tool returned but asset was unavailable. Invoker's
		// caller is responsible for treating missing assets as NotFound.
		return VesselDataTableDetail::SerializeAsCondensedJson(Root);
	}

	UScriptStruct* RowStruct = Table->GetRowStruct();
	if (!RowStruct)
	{
		return VesselDataTableDetail::SerializeAsCondensedJson(Root);
	}

	// Decide row set.
	if (RowNames.Num() == 0)
	{
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			TSharedRef<FJsonObject> RowObj = VesselDataTableDetail::RowToJsonObject(RowStruct, Pair.Value);
			Root->SetObjectField(Pair.Key.ToString(), RowObj);
		}
	}
	else
	{
		for (const FName& RowName : RowNames)
		{
			if (uint8* RowData = Table->FindRowUnchecked(RowName))
			{
				TSharedRef<FJsonObject> RowObj = VesselDataTableDetail::RowToJsonObject(RowStruct, RowData);
				Root->SetObjectField(RowName.ToString(), RowObj);
			}
		}
	}

	return VesselDataTableDetail::SerializeAsCondensedJson(Root);
}

FString UVesselDataTableTools::ReadDataTable(const FString& AssetPath, const TArray<FName>& RowNames)
{
	FSoftObjectPath Soft(AssetPath);
	UObject* Loaded = Soft.TryLoad();
	UDataTable* Table = Cast<UDataTable>(Loaded);
	if (!Table)
	{
		UE_LOG(LogVesselRegistry, Warning,
			TEXT("ReadDataTable: asset '%s' could not be loaded as UDataTable."), *AssetPath);
		return FString(TEXT("{}"));
	}
	return ReadRowsJson(Table, RowNames);
}
