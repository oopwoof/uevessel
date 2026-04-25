// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Tools/VesselValidatorTools.h"

#include "VesselLog.h"

#include "Editor.h"
#include "EditorValidatorSubsystem.h"
#include "DataValidationModule.h"
#include "Misc/DataValidation.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"

namespace VesselValidatorDetail
{
	static FString SerializeObject(const TSharedRef<FJsonObject>& Obj)
	{
		FString Out;
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Obj, Writer);
		return Out;
	}

	static TArray<TSharedPtr<FJsonValue>> TextsToJsonValues(const TArray<FText>& Texts)
	{
		TArray<TSharedPtr<FJsonValue>> Out;
		Out.Reserve(Texts.Num());
		for (const FText& T : Texts)
		{
			Out.Add(MakeShared<FJsonValueString>(T.ToString()));
		}
		return Out;
	}

	static const TCHAR* ResultToString(EDataValidationResult R)
	{
		switch (R)
		{
			case EDataValidationResult::Valid:        return TEXT("valid");
			case EDataValidationResult::Invalid:      return TEXT("invalid");
			case EDataValidationResult::NotValidated: return TEXT("not_validated");
			default:                                  return TEXT("unknown");
		}
	}
}

FString UVesselValidatorTools::RunAssetValidator(const FString& AssetPath)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset"), AssetPath);

#if WITH_EDITOR
	UEditorValidatorSubsystem* Subsys =
		GEditor ? GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>() : nullptr;
	if (!Subsys)
	{
		Root->SetBoolField(TEXT("ok"), false);
		Root->SetStringField(TEXT("error"), TEXT("EditorValidatorSubsystem unavailable"));
		return VesselValidatorDetail::SerializeObject(Root);
	}

	const FSoftObjectPath Soft(AssetPath);
	UObject* Asset = Soft.TryLoad();
	if (!Asset)
	{
		Root->SetBoolField(TEXT("ok"), false);
		Root->SetStringField(TEXT("error"), TEXT("Asset could not be loaded"));
		return VesselValidatorDetail::SerializeObject(Root);
	}

	// FDataValidationContext's 3-arg ctor signature shifted in UE 5.7. The
	// default ctor is stable across versions; usecase defaults to None, which
	// means validators that gate on Manual won't see this run as user-initiated.
	// Acceptable for v0.1 — callers (agents) classify their own intent.
	FDataValidationContext Context;

	const EDataValidationResult Result = Subsys->IsObjectValidWithContext(Asset, Context);

	TArray<FText> Errors;
	TArray<FText> Warnings;
	Context.SplitIssues(Warnings, Errors);

	Root->SetBoolField(TEXT("ok"), true);
	Root->SetStringField(TEXT("result"), VesselValidatorDetail::ResultToString(Result));
	Root->SetArrayField(TEXT("errors"), VesselValidatorDetail::TextsToJsonValues(Errors));
	Root->SetArrayField(TEXT("warnings"), VesselValidatorDetail::TextsToJsonValues(Warnings));

	UE_LOG(LogVesselRegistry, Verbose,
		TEXT("RunAssetValidator '%s' result=%s errors=%d warnings=%d"),
		*AssetPath,
		VesselValidatorDetail::ResultToString(Result),
		Errors.Num(), Warnings.Num());

	return VesselValidatorDetail::SerializeObject(Root);
#else
	Root->SetBoolField(TEXT("ok"), false);
	Root->SetStringField(TEXT("error"), TEXT("Editor-only"));
	return VesselValidatorDetail::SerializeObject(Root);
#endif
}
