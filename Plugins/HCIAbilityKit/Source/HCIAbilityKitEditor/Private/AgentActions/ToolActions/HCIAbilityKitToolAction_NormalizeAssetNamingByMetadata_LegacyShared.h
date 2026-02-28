#pragma once

#include "CoreMinimal.h"

#include "AgentActions/Support/HCIAbilityKitAssetMoveRenameUtils.h"

#include "AssetRegistry/AssetData.h"
#include "Dom/JsonObject.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
static bool HCI_TryExtractImportSourceStemFromAssetData(const FAssetData& AssetData, FString& OutStem)
{
	OutStem.Reset();

	static const TArray<FName> CandidateTags = {
		FName(TEXT("SourceFile")),
		FName(TEXT("SourceFilePath")),
		FName(TEXT("ImportedFile")),
		FName(TEXT("ImportedFilePath")),
		FName(TEXT("ImportPath")),
		FName(TEXT("SourceFileRelativePath")),
		FName(TEXT("AssetImportData"))};

	FString Raw;
	for (const FName& Tag : CandidateTags)
	{
		Raw.Reset();
		if (AssetData.GetTagValue(Tag, Raw))
		{
			Raw.TrimStartAndEndInline();
			if (!Raw.IsEmpty())
			{
				break;
			}
		}
	}

	if (Raw.IsEmpty())
	{
		return false;
	}

	// Common formats:
	// - "D:/path/file.png"
	// - (D:/path/file.png)
	// - "D:/path/file.png;D:/path/file2.png"
	Raw.ReplaceInline(TEXT("\""), TEXT(""));
	Raw.ReplaceInline(TEXT("'"), TEXT(""));
	Raw.ReplaceInline(TEXT("("), TEXT(""));
	Raw.ReplaceInline(TEXT(")"), TEXT(""));
	Raw.TrimStartAndEndInline();

	{
		int32 Sep = INDEX_NONE;
		if (Raw.FindChar(TEXT(';'), Sep) && Sep > 0)
		{
			Raw.LeftInline(Sep, false);
		}
	}
	{
		int32 Sep = INDEX_NONE;
		if (Raw.FindChar(TEXT('|'), Sep) && Sep > 0)
		{
			Raw.LeftInline(Sep, false);
		}
	}
	Raw.TrimStartAndEndInline();

	if (Raw.IsEmpty())
	{
		return false;
	}

	OutStem = FPaths::GetBaseFilename(Raw);
	OutStem.TrimStartAndEndInline();
	return !OutStem.IsEmpty();
}

// Forward declare (defined below) so AssetData-only naming path can reuse the same fallback logic.
static FString HCI_DeriveBaseNameFromCurrentName(const FString& CurrentAssetName, const FString& Prefix);

static bool HCI_TryBuildNormalizedNameBaseFromAssetData(
	const FAssetData& AssetData,
	const FString& MetadataSource,
	const FString& Prefix,
	const FString& CurrentAssetName,
	FString& OutBaseName,
	FString& OutFailureReason)
{
	OutBaseName.Reset();
	OutFailureReason.Reset();

	const bool bUseImportDataOnly = MetadataSource.Equals(TEXT("import_data_only"), ESearchCase::IgnoreCase);
	const bool bAuto = MetadataSource.Equals(TEXT("auto"), ESearchCase::IgnoreCase);

	if (!bAuto && !bUseImportDataOnly)
	{
		OutFailureReason = TEXT("metadata_source_not_supported");
		return false;
	}

	FString SourceStem;
	if (HCI_TryExtractImportSourceStemFromAssetData(AssetData, SourceStem))
	{
		OutBaseName = HCI_SanitizeIdentifier(HCI_RemoveKnownPrefixToken(SourceStem));
		if (!OutBaseName.IsEmpty())
		{
			if (OutBaseName.StartsWith(Prefix + TEXT("_"), ESearchCase::IgnoreCase))
			{
				OutBaseName.RightChopInline(Prefix.Len() + 1, false);
				OutBaseName = HCI_SanitizeIdentifier(OutBaseName);
			}
			if (!OutBaseName.IsEmpty())
			{
				return true;
			}
		}
	}

	if (bUseImportDataOnly)
	{
		OutFailureReason = TEXT("import_metadata_missing");
		return false;
	}

	OutBaseName = HCI_DeriveBaseNameFromCurrentName(CurrentAssetName, Prefix);
	if (!OutBaseName.IsEmpty())
	{
		return true;
	}

	OutFailureReason = TEXT("name_sanitization_failed");
	return false;
}

static bool HCI_TryGetAssetImportData(UObject* Asset, UAssetImportData*& OutImportData)
{
	OutImportData = nullptr;
	if (Asset == nullptr)
	{
		return false;
	}

	const FObjectProperty* ImportDataProperty = FindFProperty<FObjectProperty>(Asset->GetClass(), TEXT("AssetImportData"));
	if (ImportDataProperty == nullptr)
	{
		return false;
	}

	UObject* ValueObject = ImportDataProperty->GetObjectPropertyValue_InContainer(Asset);
	OutImportData = Cast<UAssetImportData>(ValueObject);
	return OutImportData != nullptr;
}

static bool HCI_TryExtractImportSourceStem(UObject* Asset, FString& OutStem)
{
	OutStem.Reset();
	UAssetImportData* ImportData = nullptr;
	if (!HCI_TryGetAssetImportData(Asset, ImportData) || ImportData == nullptr)
	{
		return false;
	}

	TArray<FString> SourceFiles;
	ImportData->ExtractFilenames(SourceFiles);
	for (const FString& SourceFile : SourceFiles)
	{
		const FString Trimmed = SourceFile.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			continue;
		}

		const FString Base = FPaths::GetBaseFilename(Trimmed);
		if (!Base.IsEmpty())
		{
			OutStem = Base;
			return true;
		}
	}
	return false;
}

static FString HCI_DeriveBaseNameFromCurrentName(const FString& CurrentAssetName, const FString& Prefix)
{
	FString Candidate = CurrentAssetName;
	const FString PrefixWithUnderscore = Prefix + TEXT("_");
	if (Candidate.StartsWith(PrefixWithUnderscore, ESearchCase::IgnoreCase))
	{
		Candidate.RightChopInline(PrefixWithUnderscore.Len(), false);
	}
	Candidate = HCI_RemoveKnownPrefixToken(Candidate);

	FString Sanitized = HCI_SanitizeIdentifier(Candidate);
	if (!Sanitized.IsEmpty())
	{
		return Sanitized;
	}
	return HCI_SanitizeIdentifier(CurrentAssetName);
}

static bool HCI_TryBuildNormalizedNameBase(
	UObject* Asset,
	const FString& MetadataSource,
	const FString& Prefix,
	const FString& CurrentAssetName,
	FString& OutBaseName,
	FString& OutFailureReason)
{
	OutBaseName.Reset();
	OutFailureReason.Reset();

	const bool bUseImportDataOnly = MetadataSource.Equals(TEXT("UAssetImportData"), ESearchCase::IgnoreCase);
	const bool bUseAssetUserDataOnly = MetadataSource.Equals(TEXT("AssetUserData"), ESearchCase::IgnoreCase);
	if (bUseAssetUserDataOnly)
	{
		OutFailureReason = TEXT("asset_user_data_metadata_not_supported");
		return false;
	}

	FString SourceStem;
	if (HCI_TryExtractImportSourceStem(Asset, SourceStem))
	{
		OutBaseName = HCI_SanitizeIdentifier(HCI_RemoveKnownPrefixToken(SourceStem));
		if (!OutBaseName.IsEmpty())
		{
			if (OutBaseName.StartsWith(Prefix + TEXT("_"), ESearchCase::IgnoreCase))
			{
				OutBaseName.RightChopInline(Prefix.Len() + 1, false);
				OutBaseName = HCI_SanitizeIdentifier(OutBaseName);
			}
			if (!OutBaseName.IsEmpty())
			{
				return true;
			}
		}
	}

	if (bUseImportDataOnly)
	{
		OutFailureReason = TEXT("import_metadata_missing");
		return false;
	}

	OutBaseName = HCI_DeriveBaseNameFromCurrentName(CurrentAssetName, Prefix);
	if (!OutBaseName.IsEmpty())
	{
		return true;
	}

	OutFailureReason = TEXT("name_sanitization_failed");
	return false;
}

struct FHCIAbilityKitAssetRoutingRules
{
	bool bPerAssetFolder = true;
	bool bGroupNamePascalCase = true;
	bool bDependencyAware = true;
	FString SharedRoot = TEXT("Shared");
	TMap<FString, FString> SubfoldersByPrefix;
};

static FString HCI_ToPascalCase(const FString& SanitizedUnderscoreName)
{
	TArray<FString> Tokens;
	SanitizedUnderscoreName.ParseIntoArray(Tokens, TEXT("_"), true);
	FString Out;
	for (FString Token : Tokens)
	{
		Token.TrimStartAndEndInline();
		if (Token.IsEmpty())
		{
			continue;
		}

		Token.ToLowerInline();
		if (Token.Len() >= 1)
		{
			Token[0] = FChar::ToUpper(Token[0]);
		}
		Out += Token;
	}
	return Out.IsEmpty() ? SanitizedUnderscoreName : Out;
}

static bool HCI_TryExtractMarkdownCodeFence(
	const FString& Markdown,
	const FString& FenceTag,
	FString& OutFenceContent)
{
	OutFenceContent.Reset();

	const FString OpenFence = FString::Printf(TEXT("```%s"), *FenceTag);
	const int32 Start = Markdown.Find(OpenFence, ESearchCase::IgnoreCase, ESearchDir::FromStart);
	if (Start == INDEX_NONE)
	{
		return false;
	}

	const int32 OpenEnd = Start + OpenFence.Len();
	const int32 Newline = Markdown.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, OpenEnd);
	const int32 AfterOpen = (Newline == INDEX_NONE) ? OpenEnd : (Newline + 1);

	const int32 End = Markdown.Find(TEXT("```"), ESearchCase::CaseSensitive, ESearchDir::FromStart, AfterOpen);
	if (End == INDEX_NONE || End <= AfterOpen)
	{
		return false;
	}

	OutFenceContent = Markdown.Mid(AfterOpen, End - AfterOpen);
	OutFenceContent.TrimStartAndEndInline();
	return !OutFenceContent.IsEmpty();
}

static FHCIAbilityKitAssetRoutingRules HCI_LoadAssetRoutingRules()
{
	FHCIAbilityKitAssetRoutingRules Rules;
	Rules.SubfoldersByPrefix.Add(TEXT("SM"), FString()); // meshes: group root
	Rules.SubfoldersByPrefix.Add(TEXT("SK"), FString()); // skeletal meshes: group root
	Rules.SubfoldersByPrefix.Add(TEXT("T"), TEXT("Textures"));
	Rules.SubfoldersByPrefix.Add(TEXT("M"), TEXT("Materials"));
	Rules.SubfoldersByPrefix.Add(TEXT("MI"), TEXT("Materials"));

	const FString RulesPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HCIAbilityKit/Rules/Project_Rules.md"));
	if (!FPaths::FileExists(RulesPath))
	{
		return Rules;
	}

	FString Markdown;
	if (!FFileHelper::LoadFileToString(Markdown, *RulesPath))
	{
		return Rules;
	}

	FString JsonText;
	if (!HCI_TryExtractMarkdownCodeFence(Markdown, TEXT("hci_routing_json"), JsonText))
	{
		return Rules;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return Rules;
	}

	bool bPerAssetFolder = true;
	if (Root->TryGetBoolField(TEXT("per_asset_folder"), bPerAssetFolder))
	{
		Rules.bPerAssetFolder = bPerAssetFolder;
	}

	bool bDependencyAware = true;
	if (Root->TryGetBoolField(TEXT("dependency_aware"), bDependencyAware))
	{
		Rules.bDependencyAware = bDependencyAware;
	}

	FString SharedRoot;
	if (Root->TryGetStringField(TEXT("shared_root"), SharedRoot))
	{
		SharedRoot.TrimStartAndEndInline();
		SharedRoot = HCI_SanitizeIdentifier(SharedRoot);
		if (!SharedRoot.IsEmpty())
		{
			Rules.SharedRoot = SharedRoot;
		}
	}

	FString GroupStyle;
	if (Root->TryGetStringField(TEXT("group_name_style"), GroupStyle))
	{
		GroupStyle.TrimStartAndEndInline();
		GroupStyle.ToLowerInline();
		if (GroupStyle == TEXT("sanitized") || GroupStyle == TEXT("underscore"))
		{
			Rules.bGroupNamePascalCase = false;
		}
		else if (GroupStyle == TEXT("pascal_case") || GroupStyle == TEXT("pascalcase"))
		{
			Rules.bGroupNamePascalCase = true;
		}
	}

	const TSharedPtr<FJsonObject>* Subfolders = nullptr;
	if (Root->TryGetObjectField(TEXT("subfolders"), Subfolders) && Subfolders && Subfolders->IsValid())
	{
		for (const auto& It : (*Subfolders)->Values)
		{
			if (!It.Value.IsValid() || It.Value->Type != EJson::String)
			{
				continue;
			}

			FString Key = It.Key;
			Key.TrimStartAndEndInline();
			Key.ToUpperInline();
			if (Key.IsEmpty())
			{
				continue;
			}

			FString Val = It.Value->AsString();
			Val.TrimStartAndEndInline();
			Val = HCI_SanitizeIdentifier(Val);
			Rules.SubfoldersByPrefix.Add(Key, Val);
		}
	}

	return Rules;
}

enum class EHCITextureRole : uint8
{
	Unknown = 0,
	BaseColor,
	Normal,
	ORM,
	Roughness,
	Metallic,
	AO
};

static EHCITextureRole HCI_DetectTextureRoleFromTokens(const TArray<FString>& TokensLower)
{
	if (TokensLower.Num() == 0)
	{
		return EHCITextureRole::Unknown;
	}

	const FString& Last = TokensLower.Last();
	if (Last == TEXT("bc") || Last == TEXT("basecolor") || Last == TEXT("albedo") || Last == TEXT("diffuse") || Last == TEXT("color"))
	{
		return EHCITextureRole::BaseColor;
	}
	if (Last == TEXT("n") || Last == TEXT("normal") || Last == TEXT("nrm") || Last == TEXT("nor"))
	{
		return EHCITextureRole::Normal;
	}
	if (Last == TEXT("orm") || Last == TEXT("occlusionroughnessmetallic"))
	{
		return EHCITextureRole::ORM;
	}
	if (Last == TEXT("r") || Last == TEXT("rough") || Last == TEXT("roughness") || Last == TEXT("rgh"))
	{
		return EHCITextureRole::Roughness;
	}
	if (Last == TEXT("m") || Last == TEXT("metal") || Last == TEXT("metallic") || Last == TEXT("mtl"))
	{
		return EHCITextureRole::Metallic;
	}
	if (Last == TEXT("ao") || Last == TEXT("occlusion"))
	{
		return EHCITextureRole::AO;
	}
	return EHCITextureRole::Unknown;
}

static FString HCI_TextureRoleSuffix(const EHCITextureRole Role)
{
	switch (Role)
	{
	case EHCITextureRole::BaseColor:
		return TEXT("BC");
	case EHCITextureRole::Normal:
		return TEXT("N");
	case EHCITextureRole::ORM:
		return TEXT("ORM");
	case EHCITextureRole::Roughness:
		return TEXT("R");
	case EHCITextureRole::Metallic:
		return TEXT("M");
	case EHCITextureRole::AO:
		return TEXT("AO");
	default:
		return FString();
	}
}

static void HCI_TrimCommonTrailingNoiseTokens(TArray<FString>& InOutTokensLower)
{
	while (InOutTokensLower.Num() > 0)
	{
		const FString& Last = InOutTokensLower.Last();
		if (Last == TEXT("final") || Last == TEXT("temp") || Last == TEXT("test"))
		{
			InOutTokensLower.Pop();
			continue;
		}
		if (Last.Len() >= 2 && Last[0] == TEXT('v') && FChar::IsDigit(Last[1]))
		{
			InOutTokensLower.Pop();
			continue;
		}
		break;
	}
}

static bool HCI_MoveAssetWithAssetTools(const FString& SourceAssetPath, const FString& DestinationAssetPath)
{
	return HCIAbilityKitAssetMoveRenameUtils::MoveAssetWithAssetTools(SourceAssetPath, DestinationAssetPath);
}
}

