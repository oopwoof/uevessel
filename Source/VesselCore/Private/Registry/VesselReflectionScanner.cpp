// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Registry/VesselReflectionScanner.h"

#include "VesselLog.h"

#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

namespace
{
	const TCHAR* kAgentToolMeta      = TEXT("AgentTool");
	const TCHAR* kCategoryMeta       = TEXT("ToolCategory");
	const TCHAR* kDescriptionMeta    = TEXT("ToolDescription");
	const TCHAR* kRequiresApproval   = TEXT("RequiresApproval");
	const TCHAR* kIrreversibleHint   = TEXT("IrreversibleHint");
	const TCHAR* kBatchEligible      = TEXT("BatchEligible");
	const TCHAR* kMinVesselVersion   = TEXT("MinVesselVersion");
	const TCHAR* kToolTags           = TEXT("ToolTags");

	bool ReadMetaBool(UFunction* Func, const TCHAR* Key, bool bDefault)
	{
#if WITH_EDITOR
		if (!Func->HasMetaData(Key))
		{
			return bDefault;
		}
		const FString Val = Func->GetMetaData(Key);
		return Val.Equals(TEXT("true"), ESearchCase::IgnoreCase)
			|| Val.Equals(TEXT("1"));
#else
		return bDefault;
#endif
	}

	FString ReadMetaString(UFunction* Func, const TCHAR* Key)
	{
#if WITH_EDITOR
		if (!Func->HasMetaData(Key))
		{
			return FString();
		}
		return Func->GetMetaData(Key);
#else
		return FString();
#endif
	}

	TArray<FString> ReadMetaTags(UFunction* Func)
	{
#if WITH_EDITOR
		const FString Raw = Func->HasMetaData(kToolTags) ? Func->GetMetaData(kToolTags) : FString();
		TArray<FString> Parts;
		Raw.ParseIntoArray(Parts, TEXT(","), /*CullEmpty=*/true);
		for (FString& P : Parts)
		{
			P.TrimStartAndEndInline();
		}
		return Parts;
#else
		return {};
#endif
	}

	FString EscapeJsonString(const FString& In)
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
}

FString FVesselReflectionScanner::PropertyToJsonSchema(FProperty* Prop)
{
	if (!Prop)
	{
		return TEXT("{\"type\":\"null\"}");
	}

	if (CastField<FStrProperty>(Prop))   return TEXT("{\"type\":\"string\"}");
	if (CastField<FNameProperty>(Prop))  return TEXT("{\"type\":\"string\"}");
	if (CastField<FTextProperty>(Prop))  return TEXT("{\"type\":\"string\"}");
	if (CastField<FBoolProperty>(Prop))  return TEXT("{\"type\":\"boolean\"}");

	if (CastField<FIntProperty>(Prop) || CastField<FInt64Property>(Prop)
	 || CastField<FInt16Property>(Prop) || CastField<FInt8Property>(Prop)
	 || CastField<FUInt32Property>(Prop) || CastField<FUInt64Property>(Prop)
	 || CastField<FUInt16Property>(Prop) || CastField<FByteProperty>(Prop))
	{
		return TEXT("{\"type\":\"integer\"}");
	}
	if (CastField<FFloatProperty>(Prop) || CastField<FDoubleProperty>(Prop))
	{
		return TEXT("{\"type\":\"number\"}");
	}

	if (FArrayProperty* Arr = CastField<FArrayProperty>(Prop))
	{
		const FString Inner = PropertyToJsonSchema(Arr->Inner);
		return FString::Printf(TEXT("{\"type\":\"array\",\"items\":%s}"), *Inner);
	}

	if (FMapProperty* Map = CastField<FMapProperty>(Prop))
	{
		const FString Value = PropertyToJsonSchema(Map->ValueProp);
		return FString::Printf(TEXT("{\"type\":\"object\",\"additionalProperties\":%s}"), *Value);
	}

	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
	{
		if (UEnum* Enum = EnumProp->GetEnum())
		{
			FString EnumList;
			const int32 Num = Enum->NumEnums();
			for (int32 i = 0; i < Num - 1; ++i) // last is the "MAX" synthetic
			{
				if (i > 0) EnumList += TEXT(",");
				EnumList += FString::Printf(TEXT("\"%s\""), *EscapeJsonString(Enum->GetNameStringByIndex(i)));
			}
			return FString::Printf(TEXT("{\"type\":\"string\",\"enum\":[%s]}"), *EnumList);
		}
	}

	if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
	{
		if (UScriptStruct* Struct = StructProp->Struct)
		{
			// Well-known value types first
			static const FName NAME_Guid("Guid");
			static const FName NAME_SoftObjectPath("SoftObjectPath");
			const FName StructName = Struct->GetFName();
			if (StructName == NAME_Guid)
			{
				return TEXT("{\"type\":\"string\",\"format\":\"uuid\"}");
			}
			if (StructName == NAME_SoftObjectPath)
			{
				return TEXT("{\"type\":\"string\",\"format\":\"vessel/asset-path\"}");
			}

			// Otherwise recurse one level over visible UPROPERTY fields.
			FString Fields;
			bool bFirst = true;
			for (TFieldIterator<FProperty> It(Struct); It; ++It)
			{
				if (!bFirst) Fields += TEXT(",");
				bFirst = false;
				const FString ChildSchema = PropertyToJsonSchema(*It);
				Fields += FString::Printf(TEXT("\"%s\":%s"),
					*EscapeJsonString(It->GetName()), *ChildSchema);
			}
			return FString::Printf(TEXT("{\"type\":\"object\",\"properties\":{%s}}"), *Fields);
		}
	}

	// Fallback — unknown types surface clearly so scanner output can be audited.
	return FString::Printf(TEXT("{\"type\":\"unknown\",\"cppType\":\"%s\"}"),
		*EscapeJsonString(Prop->GetCPPType()));
}

FVesselToolSchema FVesselReflectionScanner::BuildSchemaForFunction(UClass* OwningClass, UFunction* Function)
{
	FVesselToolSchema S;
	if (!Function)
	{
		return S;
	}

	S.Name = Function->GetFName();
	S.SourceFunctionName = Function->GetName();
	if (OwningClass)
	{
		S.SourceClassName = OwningClass->GetName();
		if (UPackage* Pkg = OwningClass->GetOutermost())
		{
			S.SourceModuleName = FPackageName::GetShortName(Pkg->GetFName());
		}
	}

	S.Category          = ReadMetaString(Function, kCategoryMeta);
	S.Description       = ReadMetaString(Function, kDescriptionMeta);
	S.bRequiresApproval = ReadMetaBool(Function, kRequiresApproval, /*default=*/true);
	S.bIrreversibleHint = ReadMetaBool(Function, kIrreversibleHint, false);
	S.bBatchEligible    = ReadMetaBool(Function, kBatchEligible, false);
	S.MinVesselVersion  = ReadMetaString(Function, kMinVesselVersion);
	S.Tags              = ReadMetaTags(Function);
	S.Function          = Function; // TWeakObjectPtr<UFunction> accepts raw assignment

	// Enumerate parameters.
	//   CPF_ReturnParm          → return value; emit as ReturnTypeJson, skip from Parameters list.
	//   CPF_OutParm w/o         → pure out-parameter — engine-internal output, LLM must NOT provide.
	//   CPF_ReferenceParm         Still allocated + initialized by Invoker, but not schema-surfaced.
	//   CPF_OutParm + CPF_Ref   → input-output (ref) — treat as normal input parameter.
	//   default                 → input by value.
	for (TFieldIterator<FProperty> It(Function); It; ++It)
	{
		FProperty* Prop = *It;
		if (!(Prop->PropertyFlags & CPF_Parm))
		{
			continue;
		}

		const bool bIsReturn  = (Prop->PropertyFlags & CPF_ReturnParm)    != 0;
		const bool bIsPureOut =
			((Prop->PropertyFlags & CPF_OutParm) != 0) &&
			((Prop->PropertyFlags & CPF_ReferenceParm) == 0) &&
			!bIsReturn;

		if (bIsReturn)
		{
			S.ReturnTypeJson = PropertyToJsonSchema(Prop);
			continue;
		}
		if (bIsPureOut)
		{
			// Intentionally not surfaced to the LLM. The Invoker still
			// allocates + initializes the slot so ProcessEvent is memory-safe.
			continue;
		}

		FVesselParameterSchema Param;
		Param.Name = Prop->GetFName();
		Param.TypeJson = PropertyToJsonSchema(Prop);
		Param.bRequired = true; // UPARAM(optional) support lands in step 3c
		Param.bIsReturnValue = false;
		S.Parameters.Add(MoveTemp(Param));
	}

	return S;
}

TArray<FVesselToolSchema> FVesselReflectionScanner::BuildToolSchemas()
{
	TArray<FVesselToolSchema> Out;
#if WITH_EDITOR
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		// Skip abstract / deprecated / transient generated classes where reflection
		// may not be stable.
		if (!Class || Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}

		for (TFieldIterator<UFunction> FuncIt(Class, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
		{
			UFunction* Func = *FuncIt;
			if (!Func || !Func->HasMetaData(kAgentToolMeta))
			{
				continue;
			}
			const FString& Val = Func->GetMetaData(kAgentToolMeta);
			if (!Val.Equals(TEXT("true"), ESearchCase::IgnoreCase) && !Val.Equals(TEXT("1")))
			{
				continue;
			}

			FVesselToolSchema Schema = BuildSchemaForFunction(Class, Func);
			UE_LOG(LogVesselRegistry, Verbose,
				TEXT("Discovered tool: %s::%s (category=%s)"),
				*Schema.SourceClassName, *Schema.SourceFunctionName, *Schema.Category);
			Out.Add(MoveTemp(Schema));
		}
	}
#else
	UE_LOG(LogVesselRegistry, Warning,
		TEXT("Reflection scan skipped — UFUNCTION meta is stripped in non-editor builds."));
#endif
	return Out;
}
