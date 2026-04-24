// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Registry/VesselToolInvoker.h"

#include "VesselLog.h"
#include "Registry/VesselToolRegistry.h"
#include "Registry/VesselToolSchema.h"
#include "Transaction/VesselTransactionScope.h"
#include "Util/VesselJsonSanitizer.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"

#include "UObject/Class.h"
#include "UObject/UnrealType.h"

namespace VesselInvokerDetail
{
	/** Set a single FProperty's value inside Params from a JsonValue. Returns actionable error message on failure. */
	static bool SetPropertyFromJson(FProperty* Prop, void* PropAddr, const TSharedPtr<FJsonValue>& Value,
		FString& OutError)
	{
		if (!Prop || !PropAddr)
		{
			OutError = TEXT("Internal: null property or address.");
			return false;
		}
		if (!Value.IsValid() || Value->Type == EJson::Null)
		{
			OutError = FString::Printf(TEXT("Parameter %s is null; a value of type %s is required."),
				*Prop->GetName(), *Prop->GetCPPType());
			return false;
		}

		if (FStrProperty* StrProp = CastField<FStrProperty>(Prop))
		{
			FString S;
			if (!Value->TryGetString(S))
			{
				OutError = FString::Printf(TEXT("Parameter %s expected string, got %d."),
					*Prop->GetName(), static_cast<int32>(Value->Type));
				return false;
			}
			StrProp->SetPropertyValue(PropAddr, S);
			return true;
		}
		if (FNameProperty* NameProp = CastField<FNameProperty>(Prop))
		{
			FString S;
			if (!Value->TryGetString(S))
			{
				OutError = FString::Printf(TEXT("Parameter %s expected string (for FName), got %d."),
					*Prop->GetName(), static_cast<int32>(Value->Type));
				return false;
			}
			NameProp->SetPropertyValue(PropAddr, FName(*S));
			return true;
		}
		if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
		{
			bool B = false;
			if (!Value->TryGetBool(B))
			{
				OutError = FString::Printf(TEXT("Parameter %s expected bool."), *Prop->GetName());
				return false;
			}
			BoolProp->SetPropertyValue(PropAddr, B);
			return true;
		}
		if (FIntProperty* IntProp = CastField<FIntProperty>(Prop))
		{
			double N = 0.0;
			if (!Value->TryGetNumber(N))
			{
				OutError = FString::Printf(TEXT("Parameter %s expected integer."), *Prop->GetName());
				return false;
			}
			IntProp->SetPropertyValue(PropAddr, static_cast<int32>(N));
			return true;
		}
		if (FInt64Property* Int64Prop = CastField<FInt64Property>(Prop))
		{
			double N = 0.0;
			if (!Value->TryGetNumber(N))
			{
				OutError = FString::Printf(TEXT("Parameter %s expected integer."), *Prop->GetName());
				return false;
			}
			Int64Prop->SetPropertyValue(PropAddr, static_cast<int64>(N));
			return true;
		}
		if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
		{
			double N = 0.0;
			if (!Value->TryGetNumber(N))
			{
				OutError = FString::Printf(TEXT("Parameter %s expected number."), *Prop->GetName());
				return false;
			}
			FloatProp->SetPropertyValue(PropAddr, static_cast<float>(N));
			return true;
		}
		if (FDoubleProperty* DblProp = CastField<FDoubleProperty>(Prop))
		{
			double N = 0.0;
			if (!Value->TryGetNumber(N))
			{
				OutError = FString::Printf(TEXT("Parameter %s expected number."), *Prop->GetName());
				return false;
			}
			DblProp->SetPropertyValue(PropAddr, N);
			return true;
		}

		if (FArrayProperty* ArrProp = CastField<FArrayProperty>(Prop))
		{
			const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
			if (!Value->TryGetArray(Items) || !Items)
			{
				OutError = FString::Printf(TEXT("Parameter %s expected array."), *Prop->GetName());
				return false;
			}
			FScriptArrayHelper Helper(ArrProp, PropAddr);
			Helper.EmptyAndAddValues(Items->Num());
			FProperty* InnerProp = ArrProp->Inner;
			for (int32 i = 0; i < Items->Num(); ++i)
			{
				uint8* ElemAddr = Helper.GetRawPtr(i);
				if (!SetPropertyFromJson(InnerProp, ElemAddr, (*Items)[i], OutError))
				{
					OutError = FString::Printf(TEXT("Parameter %s[%d]: %s"),
						*Prop->GetName(), i, *OutError);
					return false;
				}
			}
			return true;
		}

		OutError = FString::Printf(TEXT("Parameter %s has unsupported type %s (Step 3b only supports basic scalars + arrays)."),
			*Prop->GetName(), *Prop->GetCPPType());
		return false;
	}

	/** Serialize a single FProperty value at ValueAddr into a JsonValue. */
	static TSharedPtr<FJsonValue> PropertyToJsonValue(FProperty* Prop, const void* ValueAddr)
	{
		if (!Prop || !ValueAddr)
		{
			return MakeShared<FJsonValueNull>();
		}
		if (FStrProperty* StrProp = CastField<FStrProperty>(Prop))
		{
			return MakeShared<FJsonValueString>(StrProp->GetPropertyValue(ValueAddr));
		}
		if (FNameProperty* NameProp = CastField<FNameProperty>(Prop))
		{
			return MakeShared<FJsonValueString>(NameProp->GetPropertyValue(ValueAddr).ToString());
		}
		if (FTextProperty* TextProp = CastField<FTextProperty>(Prop))
		{
			return MakeShared<FJsonValueString>(TextProp->GetPropertyValue(ValueAddr).ToString());
		}
		if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
		{
			return MakeShared<FJsonValueBoolean>(BoolProp->GetPropertyValue(ValueAddr));
		}
		if (FIntProperty* IntProp = CastField<FIntProperty>(Prop))
		{
			return MakeShared<FJsonValueNumber>(IntProp->GetPropertyValue(ValueAddr));
		}
		if (FInt64Property* Int64Prop = CastField<FInt64Property>(Prop))
		{
			return MakeShared<FJsonValueNumber>(static_cast<double>(Int64Prop->GetPropertyValue(ValueAddr)));
		}
		if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
		{
			return MakeShared<FJsonValueNumber>(FloatProp->GetPropertyValue(ValueAddr));
		}
		if (FDoubleProperty* DblProp = CastField<FDoubleProperty>(Prop))
		{
			return MakeShared<FJsonValueNumber>(DblProp->GetPropertyValue(ValueAddr));
		}
		if (FArrayProperty* ArrProp = CastField<FArrayProperty>(Prop))
		{
			FScriptArrayHelper Helper(ArrProp, ValueAddr);
			TArray<TSharedPtr<FJsonValue>> Items;
			for (int32 i = 0; i < Helper.Num(); ++i)
			{
				Items.Add(PropertyToJsonValue(ArrProp->Inner, Helper.GetRawPtr(i)));
			}
			return MakeShared<FJsonValueArray>(Items);
		}
		return MakeShared<FJsonValueNull>();
	}
}

FVesselResult<FString> FVesselToolInvoker::Invoke(
	FName ToolName,
	const FString& ArgsJson,
	const FInvokeOptions& Options)
{
	// ProcessEvent on UObjects is not thread-safe; UE hard-asserts if called
	// from a non-game thread. Fail loudly if an async callback tries to drive
	// tool dispatch directly — callers must hop back via TaskGraph / AsyncTask.
	checkf(IsInGameThread(),
		TEXT("FVesselToolInvoker::Invoke must be called on the Game Thread "
		     "(ProcessEvent requirement). Dispatch via AsyncTask(ENamedThreads::GameThread, ...) if needed."));

	const FVesselToolSchema* Schema = FVesselToolRegistry::Get().FindSchema(ToolName);
	if (!Schema)
	{
		return FVesselResult<FString>::Err(EVesselResultCode::NotFound,
			FString::Printf(TEXT("Tool '%s' not found in registry. "
			                     "Use VesselRegistry.List to see available tools."),
				*ToolName.ToString()));
	}

	// TWeakObjectPtr detects the Live Coding / module reload case where the
	// UFunction was recreated. Rescan is the recovery path.
	UFunction* Func = Schema->Function.Get();
	if (!Func)
	{
		return FVesselResult<FString>::Err(EVesselResultCode::Internal,
			FString::Printf(TEXT("Tool '%s' has a stale UFunction (likely Live Coding / module reload). "
			                     "Run VesselRegistry.Refresh or restart the Editor."),
				*ToolName.ToString()));
	}

	// Parse + sanitize args JSON.
	TSharedPtr<FJsonObject> ArgsObj;
	if (!ArgsJson.TrimStartAndEnd().IsEmpty())
	{
		if (!FVesselJsonSanitizer::ParseAsObject(ArgsJson, ArgsObj) || !ArgsObj.IsValid())
		{
			return FVesselResult<FString>::Err(EVesselResultCode::ValidationError,
				TEXT("ArgsJson did not parse to an object. Provide exactly one JSON object, no prose."));
		}
	}
	else
	{
		ArgsObj = MakeShared<FJsonObject>();
	}

	// Allocate + init param buffer.
	const int32 ParmsSize = Func->ParmsSize;
	uint8* Parms = static_cast<uint8*>(FMemory_Alloca(ParmsSize));
	FMemory::Memzero(Parms, ParmsSize);

	// Initialize all param slots (strings / arrays have non-trivial ctors).
	for (TFieldIterator<FProperty> It(Func); It; ++It)
	{
		if (It->HasAllPropertyFlags(CPF_Parm))
		{
			It->InitializeValue_InContainer(Parms);
		}
	}

	FVesselResult<FString> OutResult;
	FProperty* ReturnProp = Func->GetReturnProperty();

	// Marshal inputs.
	bool bMarshalOk = true;
	FString MarshalError;
	for (TFieldIterator<FProperty> It(Func); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop->HasAllPropertyFlags(CPF_Parm))
		{
			continue;
		}
		if (Prop->HasAllPropertyFlags(CPF_ReturnParm))
		{
			continue;
		}

		const TSharedPtr<FJsonValue>* JsonField = ArgsObj->Values.Find(Prop->GetName());
		if (!JsonField || !(*JsonField).IsValid())
		{
			bMarshalOk = false;
			MarshalError = FString::Printf(TEXT("Missing required parameter '%s' (type %s)."),
				*Prop->GetName(), *Prop->GetCPPType());
			break;
		}

		if (!VesselInvokerDetail::SetPropertyFromJson(
				Prop, Prop->ContainerPtrToValuePtr<void>(Parms), *JsonField, MarshalError))
		{
			bMarshalOk = false;
			break;
		}
	}

	if (!bMarshalOk)
	{
		// Destroy init'd values before bail.
		for (TFieldIterator<FProperty> It(Func); It; ++It)
		{
			if (It->HasAllPropertyFlags(CPF_Parm))
			{
				It->DestroyValue_InContainer(Parms);
			}
		}
		return FVesselResult<FString>::Err(EVesselResultCode::ValidationError, MarshalError);
	}

	// Policy-scoped transaction.
	FVesselTransactionScope TxScope(*Schema, Options.SessionId);

	// Execute on CDO.
	UClass* OwningClass = Func->GetOuterUClass();
	UObject* CDO = OwningClass ? OwningClass->GetDefaultObject() : nullptr;
	if (!CDO)
	{
		for (TFieldIterator<FProperty> It(Func); It; ++It)
		{
			if (It->HasAllPropertyFlags(CPF_Parm))
			{
				It->DestroyValue_InContainer(Parms);
			}
		}
		return FVesselResult<FString>::Err(EVesselResultCode::Internal,
			TEXT("Tool's owning class has no CDO — cannot invoke."));
	}

	CDO->ProcessEvent(Func, Parms);

	// Serialize return value as JSON string.
	FString ReturnJson;
	if (ReturnProp)
	{
		TSharedPtr<FJsonValue> RetValue = VesselInvokerDetail::PropertyToJsonValue(
			ReturnProp, ReturnProp->ContainerPtrToValuePtr<void>(Parms));

		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ReturnJson);
		FJsonSerializer::Serialize(RetValue.ToSharedRef(), FString(), Writer);
	}
	else
	{
		ReturnJson = TEXT("null");
	}

	// Teardown param buffer.
	for (TFieldIterator<FProperty> It(Func); It; ++It)
	{
		if (It->HasAllPropertyFlags(CPF_Parm))
		{
			It->DestroyValue_InContainer(Parms);
		}
	}

	return FVesselResult<FString>::Ok(MoveTemp(ReturnJson));
}
