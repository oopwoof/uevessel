// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Tools/VesselAssetTools.h"

#include "VesselLog.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/ARFilter.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"

namespace VesselAssetDetail
{
	static IAssetRegistry* GetAssetRegistry()
	{
		FAssetRegistryModule& Module = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
			TEXT("AssetRegistry"));
		return &Module.Get();
	}

	static FString SerializeArray(const TArray<TSharedPtr<FJsonValue>>& Arr)
	{
		FString Out;
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Arr, Writer);
		return Out;
	}

	static FString SerializeObject(const TSharedRef<FJsonObject>& Obj)
	{
		FString Out;
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Obj, Writer);
		return Out;
	}
}

FString UVesselAssetTools::ListAssets(const FString& ContentPath, bool bRecursive)
{
	IAssetRegistry* Registry = VesselAssetDetail::GetAssetRegistry();
	if (!Registry)
	{
		return TEXT("[]");
	}

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*ContentPath));
	Filter.bRecursivePaths = bRecursive;

	TArray<FAssetData> Assets;
	Registry->GetAssets(Filter, Assets);

	TArray<TSharedPtr<FJsonValue>> Items;
	Items.Reserve(Assets.Num());
	for (const FAssetData& A : Assets)
	{
		Items.Add(MakeShared<FJsonValueString>(A.GetSoftObjectPath().ToString()));
	}

	UE_LOG(LogVesselRegistry, Verbose,
		TEXT("ListAssets '%s' recursive=%d -> %d entries"),
		*ContentPath, bRecursive ? 1 : 0, Items.Num());

	return VesselAssetDetail::SerializeArray(Items);
}

FString UVesselAssetTools::ReadAssetMetadata(const FString& AssetPath)
{
	IAssetRegistry* Registry = VesselAssetDetail::GetAssetRegistry();
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);

	if (!Registry)
	{
		Root->SetBoolField(TEXT("found"), false);
		Root->SetStringField(TEXT("error"), TEXT("AssetRegistry unavailable"));
		return VesselAssetDetail::SerializeObject(Root);
	}

	const FSoftObjectPath Soft(AssetPath);
	const FAssetData Data = Registry->GetAssetByObjectPath(Soft);
	if (!Data.IsValid())
	{
		Root->SetBoolField(TEXT("found"), false);
		return VesselAssetDetail::SerializeObject(Root);
	}

	Root->SetBoolField(TEXT("found"), true);
	Root->SetStringField(TEXT("class"), Data.AssetClassPath.ToString());
	Root->SetStringField(TEXT("package"), Data.PackageName.ToString());
	Root->SetStringField(TEXT("asset_name"), Data.AssetName.ToString());
	Root->SetBoolField(TEXT("is_redirector"), Data.IsRedirector());

	// Surface asset registry tags so LLM can see things like ParentClass,
	// PrimaryAssetId, etc. without loading the asset.
	TSharedRef<FJsonObject> Tags = MakeShared<FJsonObject>();
	for (const TPair<FName, FAssetTagValueRef>& Pair : Data.TagsAndValues)
	{
		Tags->SetStringField(Pair.Key.ToString(), Pair.Value.AsString());
	}
	Root->SetObjectField(TEXT("tags"), Tags);

	return VesselAssetDetail::SerializeObject(Root);
}
