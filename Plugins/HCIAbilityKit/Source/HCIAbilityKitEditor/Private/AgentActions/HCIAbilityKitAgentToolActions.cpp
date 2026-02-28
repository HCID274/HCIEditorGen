#include "AgentActions/HCIAbilityKitAgentToolActions.h"

#include "AgentActions/Support/HCIAbilityKitAssetMoveRenameUtils.h"
#include "AgentActions/Support/HCIAbilityKitAssetNamingRules.h"
#include "AgentActions/Support/HCIAbilityKitAssetPathUtils.h"
#include "AgentActions/Support/HCIAbilityKitRedirectorFixup.h"
#include "AgentActions/ToolActions/HCIAbilityKitToolActionFactories.h"

#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "EditorFramework/AssetImportData.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/UnrealType.h"
#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitAgentToolActions, Log, All);

namespace
{
static bool HCI_TrySplitObjectPath(
	const FString& ObjectPath,
	FString& OutPackagePath,
	FString& OutAssetName)
{
	return HCIAbilityKitAssetPathUtils::TrySplitObjectPath(ObjectPath, OutPackagePath, OutAssetName);
}

static FString HCI_ToObjectPath(const FString& PackagePath, const FString& AssetName)
{
	return HCIAbilityKitAssetPathUtils::ToObjectPath(PackagePath, AssetName);
}

static void HCI_NormalizeAssetPathVariants(
	const FString& InPath,
	FString& OutAssetPath,
	FString& OutObjectPath)
{
	HCIAbilityKitAssetPathUtils::NormalizeAssetPathVariants(InPath, OutAssetPath, OutObjectPath);
}

static FString HCI_GetDirectoryFromPackagePath(const FString& PackagePath)
{
	return HCIAbilityKitAssetPathUtils::GetDirectoryFromPackagePath(PackagePath);
}

static FString HCI_TrimTrailingSlash(const FString& InPath)
{
	return HCIAbilityKitAssetPathUtils::TrimTrailingSlash(InPath);
}

static FString HCI_SanitizeIdentifier(const FString& InText)
{
	return HCIAbilityKitAssetNamingRules::SanitizeIdentifier(InText);
}

static FString HCI_RemoveKnownPrefixToken(const FString& InName)
{
	return HCIAbilityKitAssetNamingRules::RemoveKnownPrefixToken(InName);
}

static FString HCI_DeriveClassPrefix(const UObject* Asset)
{
	return HCIAbilityKitAssetNamingRules::DeriveClassPrefix(Asset);
}

static FString HCI_DeriveClassPrefixFromAssetData(const FAssetData& AssetData)
{
	return HCIAbilityKitAssetNamingRules::DeriveClassPrefixFromAssetData(AssetData);
}

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

static bool HCI_TryReadRequiredStringArg(
	const TSharedPtr<FJsonObject>& Args,
	const TCHAR* Field,
	FString& OutValue)
{
	OutValue.Reset();
	if (!Args.IsValid())
	{
		return false;
	}
	if (!Args->TryGetStringField(Field, OutValue))
	{
		return false;
	}
	OutValue.TrimStartAndEndInline();
	return !OutValue.IsEmpty();
}

static bool HCI_TryReadRequiredStringArrayArg(
	const TSharedPtr<FJsonObject>& Args,
	const TCHAR* Field,
	TArray<FString>& OutValue)
{
	OutValue.Reset();
	if (!Args.IsValid())
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* JsonArray = nullptr;
	if (!Args->TryGetArrayField(Field, JsonArray))
	{
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Val : *JsonArray)
	{
		if (Val.IsValid() && Val->Type == EJson::String)
		{
			OutValue.Add(Val->AsString());
		}
	}
	return OutValue.Num() > 0;
}

static bool HCI_TryReadOptionalStringArrayArg(
	const TSharedPtr<FJsonObject>& Args,
	const TCHAR* Field,
	TArray<FString>& OutValue)
{
	OutValue.Reset();
	if (!Args.IsValid() || !Args->HasField(Field))
	{
		return true;
	}
	const TArray<TSharedPtr<FJsonValue>>* JsonArray = nullptr;
	if (!Args->TryGetArrayField(Field, JsonArray))
	{
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Val : *JsonArray)
	{
		if (!Val.IsValid() || Val->Type != EJson::String)
		{
			return false;
		}
		FString Item = Val->AsString();
		Item.TrimStartAndEndInline();
		if (!Item.IsEmpty())
		{
			OutValue.Add(MoveTemp(Item));
		}
	}
	return true;
}

static bool HCI_TryReadRequiredIntArg(
	const TSharedPtr<FJsonObject>& Args,
	const TCHAR* Field,
	int32& OutValue)
{
	if (!Args.IsValid())
	{
		return false;
	}
	return Args->TryGetNumberField(Field, OutValue);
}

#if 0 // moved to Private/AgentActions/ToolActions/*.cpp (Global Step2 ToolActions split)
static bool HCI_ValidateSourceAssetExists(
	const FString& SourceAssetPath,
	FHCIAbilityKitAgentToolActionResult& OutResult)
{
	const HCIAbilityKitAssetMoveRenameUtils::FHCIAbilityKitValidateAssetExistsResult Check =
		HCIAbilityKitAssetMoveRenameUtils::ValidateSourceAssetExists(SourceAssetPath);
	if (!Check.bOk)
	{
		OutResult.bSucceeded = false;
		OutResult.ErrorCode = Check.ErrorCode.IsEmpty() ? TEXT("E4201") : Check.ErrorCode;
		OutResult.Reason = Check.Reason.IsEmpty() ? TEXT("asset_not_found") : Check.Reason;
		return false;
	}
	return true;
}

static FString HCI_GetDirectoryLeafName(const FString& Path)
{
	return HCIAbilityKitAssetPathUtils::GetDirectoryLeafName(Path);
}

static FString HCI_NormalizeFuzzyToken(const FString& Text)
{
	FString Normalized = Text.ToLower();
	Normalized.ReplaceInline(TEXT("_"), TEXT(""), ESearchCase::CaseSensitive);
	Normalized.ReplaceInline(TEXT(" "), TEXT(""), ESearchCase::CaseSensitive);
	Normalized.ReplaceInline(TEXT("-"), TEXT(""), ESearchCase::CaseSensitive);
	return Normalized;
}

static void HCI_AddUniqueSearchKeyword(TArray<FString>& OutKeywords, const FString& Keyword)
{
	FString Trimmed = Keyword;
	Trimmed.TrimStartAndEndInline();
	if (Trimmed.IsEmpty())
	{
		return;
	}

	for (const FString& Existing : OutKeywords)
	{
		if (Existing.Equals(Trimmed, ESearchCase::IgnoreCase))
		{
			return;
		}
	}
	OutKeywords.Add(Trimmed);
}

static FString HCI_RemoveDirectorySemanticSuffix(const FString& InKeyword)
{
	FString Out = InKeyword;
	Out.ReplaceInline(TEXT("目录"), TEXT(""), ESearchCase::CaseSensitive);
	Out.ReplaceInline(TEXT("文件夹"), TEXT(""), ESearchCase::CaseSensitive);
	Out.ReplaceInline(TEXT("folder"), TEXT(""), ESearchCase::IgnoreCase);
	Out.ReplaceInline(TEXT("directory"), TEXT(""), ESearchCase::IgnoreCase);
	Out.TrimStartAndEndInline();
	return Out;
}

static TArray<FString> HCI_ExpandSearchKeywords(const FString& RawKeyword)
{
	TArray<FString> Expanded;
	HCI_AddUniqueSearchKeyword(Expanded, RawKeyword);

	const FString Stripped = HCI_RemoveDirectorySemanticSuffix(RawKeyword);
	HCI_AddUniqueSearchKeyword(Expanded, Stripped);

	const FString Normalized = HCI_NormalizeFuzzyToken(RawKeyword);
	if (Normalized.Contains(TEXT("临时")) || Normalized.Contains(TEXT("temp")) || Normalized.Contains(TEXT("tmp")))
	{
		HCI_AddUniqueSearchKeyword(Expanded, TEXT("Temp"));
	}
	if (Normalized.Contains(TEXT("艺术")) || Normalized.Contains(TEXT("美术")) || Normalized.Contains(TEXT("art")))
	{
		HCI_AddUniqueSearchKeyword(Expanded, TEXT("Art"));
	}
	if (Normalized.Contains(TEXT("关卡")) || Normalized.Contains(TEXT("场景")) || Normalized.Contains(TEXT("level")) || Normalized.Contains(TEXT("map")))
	{
		HCI_AddUniqueSearchKeyword(Expanded, TEXT("Maps"));
	}
	if (Normalized.Contains(TEXT("角色")) || Normalized.Contains(TEXT("character")))
	{
		HCI_AddUniqueSearchKeyword(Expanded, TEXT("Character"));
	}

	if (Expanded.Num() == 0)
	{
		HCI_AddUniqueSearchKeyword(Expanded, TEXT("Temp"));
	}
	return Expanded;
}

static bool HCI_TryResolveSemanticFallbackDirectory(const TArray<FString>& SearchKeywords, FString& OutDirectory)
{
	OutDirectory.Reset();
	for (const FString& Keyword : SearchKeywords)
	{
		const FString Normalized = HCI_NormalizeFuzzyToken(Keyword);
		if (Normalized.IsEmpty())
		{
			continue;
		}
		if (Normalized.Contains(TEXT("temp")) || Normalized.Contains(TEXT("tmp")) || Normalized.Contains(TEXT("临时")))
		{
			OutDirectory = TEXT("/Game/Temp");
			return true;
		}
		if (Normalized.Contains(TEXT("art")) || Normalized.Contains(TEXT("艺术")) || Normalized.Contains(TEXT("美术")))
		{
			OutDirectory = TEXT("/Game/Art");
			return true;
		}
		if (Normalized.Contains(TEXT("map")) || Normalized.Contains(TEXT("level")) || Normalized.Contains(TEXT("关卡")) || Normalized.Contains(TEXT("场景")))
		{
			OutDirectory = TEXT("/Game/Maps");
			return true;
		}
	}
	return false;
}

static int32 HCI_ComputePathKeywordScore(const FString& Path, const FString& Keyword)
{
	const FString KeywordNormalized = HCI_NormalizeFuzzyToken(Keyword);
	const FString PathNormalized = HCI_NormalizeFuzzyToken(Path);
	const FString Leaf = HCI_GetDirectoryLeafName(Path);
	const FString LeafNormalized = HCI_NormalizeFuzzyToken(Leaf);

	if (KeywordNormalized.IsEmpty())
	{
		return 0;
	}

	const bool bLeafExact = (LeafNormalized == KeywordNormalized);
	const bool bLeafContainsKeyword = LeafNormalized.Contains(KeywordNormalized);
	const bool bKeywordContainsLeaf = (LeafNormalized.Len() >= 3) && KeywordNormalized.Contains(LeafNormalized);
	const bool bPathContainsKeyword = PathNormalized.Contains(KeywordNormalized);
	if (!bLeafExact && !bLeafContainsKeyword && !bKeywordContainsLeaf && !bPathContainsKeyword)
	{
		return 0;
	}

	int32 Score = 0;
	if (bLeafExact)
	{
		Score += 1400;
	}
	if (LeafNormalized.StartsWith(KeywordNormalized))
	{
		Score += 700;
	}
	if (bLeafContainsKeyword)
	{
		Score += 550;
	}
	if (bKeywordContainsLeaf)
	{
		Score += 500;
	}
	if (bPathContainsKeyword)
	{
		Score += 350;
	}
	if (Path.EndsWith(Leaf))
	{
		Score += 250;
	}
	Score += FMath::Min(80, KeywordNormalized.Len() * 4);
	return Score;
}

static void HCI_FixupRedirectorReferencers(
	const FString& SourceAssetPath,
	FHCIAbilityKitAgentToolActionResult& OutResult)
{
	const HCIAbilityKitRedirectorFixup::FHCIAbilityKitRedirectorFixupResult Fixup =
		HCIAbilityKitRedirectorFixup::FixupRedirectorReferencers(SourceAssetPath);
	OutResult.Evidence.Add(TEXT("redirector_fixup"), Fixup.Status.IsEmpty() ? TEXT("unknown") : Fixup.Status);
	if (Fixup.Status != TEXT("no_redirector_found"))
	{
		OutResult.Evidence.Add(TEXT("redirector_count"), FString::FromInt(Fixup.RedirectorCount));
	}
}

static bool HCI_MoveAssetWithAssetTools(const FString& SourceAssetPath, const FString& DestinationAssetPath)
{
	return HCIAbilityKitAssetMoveRenameUtils::MoveAssetWithAssetTools(SourceAssetPath, DestinationAssetPath);
}

class FHCIAbilityKitSearchPathToolAction final : public IHCIAbilityKitAgentToolAction
{
public:
	virtual FName GetToolName() const override
	{
		return TEXT("SearchPath");
	}

	virtual bool DryRun(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult);
	}

	virtual bool Execute(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult);
	}

private:
	struct FHCIPathCandidate
	{
		FString Path;
		int32 Score = 0;
		FString MatchedKeyword;
	};

	static bool RunInternal(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult)
	{
		FString Keyword;
		if (!HCI_TryReadRequiredStringArg(Request.Args, TEXT("keyword"), Keyword))
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4001");
			OutResult.Reason = TEXT("required_arg_missing");
			return false;
		}

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& Registry = AssetRegistryModule.Get();

		if (!Registry.IsSearchAllAssets())
		{
			UE_LOG(
				LogHCIAbilityKitAgentToolActions,
				Display,
				TEXT("[HCIAbilityKit][SearchPath] search_all_assets=start keyword=\"%s\""),
				*Keyword);
			Registry.SearchAllAssets(true);
		}

		if (Registry.IsLoadingAssets())
		{
			Registry.WaitForCompletion();
		}

		TArray<FHCIPathCandidate> Candidates;
		int32 ScannedGamePathCount = 0;
		const FString KeywordNormalized = HCI_NormalizeFuzzyToken(Keyword);
		const TArray<FString> SearchKeywords = HCI_ExpandSearchKeywords(Keyword);
		const FString ExpandedKeywordsLog = FString::Join(SearchKeywords, TEXT("|"));
		Registry.EnumerateAllCachedPaths([&](FName PathName)
		{
			const FString Path = PathName.ToString();
			if (!Path.StartsWith(TEXT("/Game/")))
			{
				return true;
			}
			++ScannedGamePathCount;

			const FString LeafName = HCI_GetDirectoryLeafName(Path);
			const FString LeafNormalized = HCI_NormalizeFuzzyToken(LeafName);
			int32 BestScore = 0;
			FString BestKeyword;
			for (const FString& SearchKeyword : SearchKeywords)
			{
				const int32 Score = HCI_ComputePathKeywordScore(Path, SearchKeyword);
				if (Score > BestScore)
				{
					BestScore = Score;
					BestKeyword = SearchKeyword;
				}
			}
			if (BestScore <= 0)
			{
				return true;
			}

			FHCIPathCandidate& Candidate = Candidates.AddDefaulted_GetRef();
			Candidate.Path = Path;
			Candidate.Score = BestScore;
			Candidate.MatchedKeyword = BestKeyword;
			UE_LOG(
				LogHCIAbilityKitAgentToolActions,
				Display,
				TEXT("[HCIAbilityKit][SearchPath] keyword=\"%s\" normalized=\"%s\" matched_keyword=\"%s\" matched_path=\"%s\" leaf=\"%s\" leaf_normalized=\"%s\" score=%d"),
				*Keyword,
				*KeywordNormalized,
				*BestKeyword,
				*Path,
				*LeafName,
				*LeafNormalized,
				BestScore);
			return true;
		});

		Candidates.Sort([](const FHCIPathCandidate& A, const FHCIPathCandidate& B)
		{
			if (A.Score != B.Score)
			{
				return A.Score > B.Score;
			}
			if (A.Path.Len() != B.Path.Len())
			{
				return A.Path.Len() < B.Path.Len();
			}
			return A.Path < B.Path;
		});

		TArray<FString> MatchedDirectories;
		MatchedDirectories.Reserve(3);
		for (const FHCIPathCandidate& Candidate : Candidates)
		{
			if (MatchedDirectories.Num() >= 3)
			{
				break;
			}
			MatchedDirectories.Add(Candidate.Path);
		}

		FString SemanticFallbackDirectory;
		const bool bUsedSemanticFallback = (MatchedDirectories.Num() == 0) &&
			HCI_TryResolveSemanticFallbackDirectory(SearchKeywords, SemanticFallbackDirectory) &&
			!SemanticFallbackDirectory.IsEmpty();
		if (bUsedSemanticFallback)
		{
			MatchedDirectories.Add(SemanticFallbackDirectory);
			UE_LOG(
				LogHCIAbilityKitAgentToolActions,
				Display,
				TEXT("[HCIAbilityKit][SearchPath] semantic_fallback keyword=\"%s\" fallback_directory=\"%s\""),
				*Keyword,
				*SemanticFallbackDirectory);
		}

		OutResult = FHCIAbilityKitAgentToolActionResult();
		OutResult.bSucceeded = true;
		OutResult.Reason = TEXT("search_path_ok");
		OutResult.EstimatedAffectedCount = MatchedDirectories.Num();
		OutResult.Evidence.Add(TEXT("keyword"), Keyword);
		OutResult.Evidence.Add(TEXT("keyword_expanded"), ExpandedKeywordsLog);
		OutResult.Evidence.Add(TEXT("keyword_expanded_count"), FString::FromInt(SearchKeywords.Num()));
		OutResult.Evidence.Add(TEXT("matched_count"), FString::FromInt(MatchedDirectories.Num()));
		OutResult.Evidence.Add(TEXT("matched_directories"), FString::Join(MatchedDirectories, TEXT("|")));
		OutResult.Evidence.Add(TEXT("best_directory"), MatchedDirectories.Num() > 0 ? MatchedDirectories[0] : TEXT("-"));
		OutResult.Evidence.Add(TEXT("semantic_fallback_used"), bUsedSemanticFallback ? TEXT("true") : TEXT("false"));
		OutResult.Evidence.Add(TEXT("semantic_fallback_directory"), bUsedSemanticFallback ? SemanticFallbackDirectory : TEXT("-"));
		OutResult.Evidence.Add(TEXT("keyword_normalized"), KeywordNormalized);
		OutResult.Evidence.Add(TEXT("scanned_game_paths"), FString::FromInt(ScannedGamePathCount));
		for (int32 Index = 0; Index < MatchedDirectories.Num(); ++Index)
		{
			OutResult.Evidence.Add(
				FString::Printf(TEXT("matched_directories[%d]"), Index),
				MatchedDirectories[Index]);
		}
		OutResult.Evidence.Add(TEXT("result"), TEXT("search_path_ok"));
		UE_LOG(
			LogHCIAbilityKitAgentToolActions,
			Display,
			TEXT("[HCIAbilityKit][SearchPath] done keyword=\"%s\" normalized=\"%s\" expanded=\"%s\" scanned_game_paths=%d matched_count=%d best_directory=\"%s\""),
			*Keyword,
			*KeywordNormalized,
			*ExpandedKeywordsLog,
			ScannedGamePathCount,
			MatchedDirectories.Num(),
			MatchedDirectories.Num() > 0 ? *MatchedDirectories[0] : TEXT("-"));
		return true;
	}
};

class FHCIAbilityKitAutoMaterialSetupByNameContractToolAction final : public IHCIAbilityKitAgentToolAction
{
public:
	virtual FName GetToolName() const override
	{
		return TEXT("AutoMaterialSetupByNameContract");
	}

	virtual bool DryRun(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult, true);
	}

	virtual bool Execute(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult, false);
	}

private:
	struct FHCIAbilityKitMatLinkFixtureReport
	{
		FString SnapshotMasterMaterialObjectPath;
		TArray<FString> TextureParameterNames;
	};

	struct FHCIContractGroup
	{
		FString Id;
		FString MeshObjectPath;
		FString TextureBCObjectPath;
		FString TextureNObjectPath;
		FString TextureORMObjectPath;
	};

	static bool HCI_TryLoadMatLinkFixtureReport(FHCIAbilityKitMatLinkFixtureReport& OutReport)
	{
		OutReport = FHCIAbilityKitMatLinkFixtureReport();

		const FString ReportPath = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("HCIAbilityKit/TestFixtures/MatLink/latest.json"));

		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *ReportPath))
		{
			return false;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return false;
		}

		Root->TryGetStringField(TEXT("snapshot_master_material_object_path"), OutReport.SnapshotMasterMaterialObjectPath);

		const TArray<TSharedPtr<FJsonValue>>* Params = nullptr;
		if (Root->TryGetArrayField(TEXT("texture_parameter_names"), Params) && Params != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& Val : *Params)
			{
				if (Val.IsValid() && Val->Type == EJson::String)
				{
					FString Name = Val->AsString();
					Name.TrimStartAndEndInline();
					if (!Name.IsEmpty())
					{
						OutReport.TextureParameterNames.Add(MoveTemp(Name));
					}
				}
			}
		}

		return true;
	}

	static FString HCI_ExtractAssetNameFromObjectOrAssetPath(const FString& InPath)
	{
		FString PackagePath;
		FString AssetName;
		if (HCI_TrySplitObjectPath(InPath, PackagePath, AssetName))
		{
			return AssetName;
		}
		return FString();
	}

	static bool HCI_TryParseContractMeshId(const FString& AssetName, FString& OutId)
	{
		OutId.Reset();
		if (!AssetName.StartsWith(TEXT("SM_")) || AssetName.Len() <= 3)
		{
			return false;
		}

		OutId = AssetName.Mid(3);
		OutId.TrimStartAndEndInline();
		OutId = HCI_SanitizeIdentifier(OutId);
		return !OutId.IsEmpty();
	}

	static bool HCI_TryParseContractTextureIdAndRole(const FString& AssetName, FString& OutId, FString& OutRole)
	{
		OutId.Reset();
		OutRole.Reset();

		if (!AssetName.StartsWith(TEXT("T_")) || AssetName.Len() <= 2)
		{
			return false;
		}

		TArray<FString> Parts;
		AssetName.ParseIntoArray(Parts, TEXT("_"), true);
		if (Parts.Num() < 3)
		{
			return false;
		}

		// T_<ID>_<ROLE>
		const FString Role = Parts.Last().ToUpper();
		if (!(Role == TEXT("BC") || Role == TEXT("N") || Role == TEXT("ORM") || Role == TEXT("RMA") || Role == TEXT("ARM")))
		{
			return false;
		}

		FString Id;
		for (int32 Index = 1; Index < Parts.Num() - 1; ++Index)
		{
			if (!Id.IsEmpty())
			{
				Id += TEXT("_");
			}
			Id += Parts[Index];
		}

		Id = HCI_SanitizeIdentifier(Id);
		if (Id.IsEmpty())
		{
			return false;
		}

		OutId = MoveTemp(Id);
		OutRole = (Role == TEXT("RMA") || Role == TEXT("ARM")) ? TEXT("ORM") : Role;
		return true;
	}

	static FString HCI_BuildStrictContractOrphanReason(const FString& AssetName)
	{
		if (AssetName.StartsWith(TEXT("T_")))
		{
			TArray<FString> Parts;
			AssetName.ParseIntoArray(Parts, TEXT("_"), true);
			const FString Tail = Parts.Num() > 0 ? Parts.Last().ToUpper() : FString();
			if (Tail == TEXT("COLOR") || Tail == TEXT("BASECOLOR") || Tail == TEXT("ALBEDO") || Tail == TEXT("DIFFUSE"))
			{
				return TEXT("贴图后缀不满足契约（末尾为 Color，期望: _BC）");
			}
			if (Tail == TEXT("NORMAL") || Tail == TEXT("NORMALMAP"))
			{
				return TEXT("贴图后缀不满足契约（末尾为 Normal，期望: _N）");
			}
			if (Tail.Contains(TEXT("ORM")) || Tail.Contains(TEXT("RMA")) || Tail.Contains(TEXT("ARM")))
			{
				return TEXT("贴图后缀不满足契约（期望: _ORM 或 _RMA）");
			}
			return TEXT("贴图命名不满足严格契约（期望: T_<ID>_BC / T_<ID>_N / T_<ID>_ORM）");
		}

		if (AssetName.StartsWith(TEXT("SM_")))
		{
			return TEXT("Mesh 命名不满足严格契约（期望: SM_<ID>）");
		}

		return TEXT("命名不满足严格契约（期望: SM_<ID> 或 T_<ID>_BC/N/ORM）");
	}

	static FString HCI_BuildReasonEvidenceString(const TMap<FString, FString>& ReasonByPath)
	{
		if (ReasonByPath.Num() <= 0)
		{
			return TEXT("none");
		}

		TArray<FString> Keys;
		ReasonByPath.GetKeys(Keys);
		Keys.Sort([](const FString& A, const FString& B) { return A < B; });

		TArray<FString> Rows;
		Rows.Reserve(Keys.Num());
		for (const FString& Key : Keys)
		{
			const FString* Reason = ReasonByPath.Find(Key);
			if (Reason == nullptr || Reason->IsEmpty())
			{
				continue;
			}
			Rows.Add(FString::Printf(TEXT("%s => %s"), *Key, **Reason));
		}

		return Rows.Num() > 0 ? FString::Join(Rows, TEXT(" | ")) : TEXT("none");
	}

	static void HCI_SelectTextureParameterNamesForRoles(
		const TArray<FString>& InCandidates,
		TArray<FName>& OutBaseColorParams,
		TArray<FName>& OutNormalParams,
		TArray<FName>& OutOrmParams)
	{
		OutBaseColorParams.Reset();
		OutNormalParams.Reset();
		OutOrmParams.Reset();

		auto ScoreParam = [](const FString& NameLower) -> int32
		{
			if (NameLower.Contains(TEXT("basecolor")))
			{
				return 100;
			}
			if (NameLower.Contains(TEXT("albedo")) || NameLower.Contains(TEXT("diffuse")) || NameLower.Contains(TEXT("color")))
			{
				return 80;
			}
			return 0;
		};

		FString BestBase;
		int32 BestBaseScore = -1;
		for (const FString& Candidate : InCandidates)
		{
			const FString Lower = Candidate.ToLower();
			const int32 Score = ScoreParam(Lower);
			if (Score > BestBaseScore)
			{
				BestBaseScore = Score;
				BestBase = Candidate;
			}
			if (Lower.Contains(TEXT("normal")))
			{
				OutNormalParams.Add(FName(*Candidate));
			}
			if (Lower.Contains(TEXT("metallicroughness")) || Lower.Contains(TEXT("metallic")) || Lower.Contains(TEXT("roughness")) || Lower.Contains(TEXT("occlusion")) || Lower.Contains(TEXT("orm")))
			{
				OutOrmParams.Add(FName(*Candidate));
			}
		}

		if (!BestBase.IsEmpty())
		{
			OutBaseColorParams.Add(FName(*BestBase));
		}

		// De-dupe.
		auto SortUnique = [](TArray<FName>& InOut)
		{
			InOut.Sort(FNameLexicalLess());
			for (int32 Index = InOut.Num() - 1; Index > 0; --Index)
			{
				if (InOut[Index].IsEqual(InOut[Index - 1], ENameCase::CaseSensitive))
				{
					InOut.RemoveAt(Index, 1, false);
				}
			}
		};

		SortUnique(OutNormalParams);
		SortUnique(OutOrmParams);
	}

	static bool HCI_TryResolveDefaultMasterMaterialPath(FString& InOutMasterMaterialPath, TArray<FString>& OutParamNameCandidates)
	{
		OutParamNameCandidates.Reset();

		FHCIAbilityKitMatLinkFixtureReport Report;
		if (HCI_TryLoadMatLinkFixtureReport(Report))
		{
			if (!Report.SnapshotMasterMaterialObjectPath.IsEmpty())
			{
				InOutMasterMaterialPath = Report.SnapshotMasterMaterialObjectPath;
			}
			OutParamNameCandidates = MoveTemp(Report.TextureParameterNames);
		}

		if (InOutMasterMaterialPath.IsEmpty())
		{
			// Fallback for demo projects that haven't run MatLinkBuildSnapshot yet.
			InOutMasterMaterialPath = TEXT("/Game/__HCI_Test/Masters/M_MatLink_Master.M_MatLink_Master");
		}

		return InOutMasterMaterialPath.StartsWith(TEXT("/Game/"));
	}

	static bool RunInternal(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult,
		const bool bIsDryRun)
	{
		TArray<FString> AssetPaths;
		FString TargetRoot;
		if (!HCI_TryReadRequiredStringArrayArg(Request.Args, TEXT("asset_paths"), AssetPaths) ||
			!HCI_TryReadRequiredStringArg(Request.Args, TEXT("target_root"), TargetRoot))
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4001");
			OutResult.Reason = TEXT("required_arg_missing");
			return false;
		}

		TargetRoot = HCI_TrimTrailingSlash(TargetRoot);
		if (!TargetRoot.StartsWith(TEXT("/Game/")))
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4009");
			OutResult.Reason = TEXT("target_root_must_start_with_game");
			return false;
		}

		FString MasterMaterialPath;
		if (Request.Args.IsValid())
		{
			Request.Args->TryGetStringField(TEXT("master_material_path"), MasterMaterialPath);
			MasterMaterialPath.TrimStartAndEndInline();
		}

		TArray<FString> ParamNameCandidates;
		if (!HCI_TryResolveDefaultMasterMaterialPath(MasterMaterialPath, ParamNameCandidates))
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4009");
			OutResult.Reason = TEXT("master_material_path_invalid");
			return false;
		}

		UObject* MasterObj = UEditorAssetLibrary::LoadAsset(MasterMaterialPath);
		UMaterialInterface* MasterMaterial = Cast<UMaterialInterface>(MasterObj);
		if (!MasterMaterial)
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4401");
			OutResult.Reason = TEXT("master_material_not_found_or_invalid");
			OutResult.Evidence.Add(TEXT("master_material_path"), MasterMaterialPath);
			OutResult.Evidence.Add(TEXT("result"), OutResult.Reason);
			return false;
		}

		// If report doesn't provide param names (or is missing), fall back to scanning the master.
		if (ParamNameCandidates.Num() <= 0)
		{
			TArray<FMaterialParameterInfo> TextureParams;
			TArray<FGuid> TextureParamGuids;
			MasterMaterial->GetAllTextureParameterInfo(TextureParams, TextureParamGuids);
			for (const FMaterialParameterInfo& Info : TextureParams)
			{
				ParamNameCandidates.Add(Info.Name.ToString());
			}
		}

		TArray<FName> BaseColorParams;
		TArray<FName> NormalParams;
		TArray<FName> OrmParams;
		HCI_SelectTextureParameterNamesForRoles(ParamNameCandidates, BaseColorParams, NormalParams, OrmParams);

		if (BaseColorParams.Num() <= 0 || NormalParams.Num() <= 0)
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4402");
			OutResult.Reason = TEXT("master_material_texture_params_unrecognized");
			OutResult.Evidence.Add(TEXT("master_material_path"), MasterMaterialPath);
			OutResult.Evidence.Add(TEXT("result"), OutResult.Reason);
			return false;
		}

		// Build strict contract groups.
		TMap<FString, FHCIContractGroup> GroupsById;
		TArray<FString> MissingGroups;
		TArray<FString> OrphanAssets;
		TSet<FString> UnresolvedAssetSet;
		TMap<FString, FString> OrphanReasonByPath;
		TMap<FString, FString> UnresolvedReasonByPath;

		for (const FString& Path : AssetPaths)
		{
			const FString AssetName = HCI_ExtractAssetNameFromObjectOrAssetPath(Path);
			if (AssetName.IsEmpty())
			{
				continue;
			}

			FString Id;
			if (HCI_TryParseContractMeshId(AssetName, Id))
			{
				FHCIContractGroup& Group = GroupsById.FindOrAdd(Id);
				Group.Id = Id;
				if (!Group.MeshObjectPath.IsEmpty() && !Group.MeshObjectPath.Equals(Path, ESearchCase::CaseSensitive))
				{
					MissingGroups.Add(FString::Printf(TEXT("%s (duplicate_mesh)"), *Id));
					continue;
				}
				Group.MeshObjectPath = Path;
				continue;
			}

			FString Role;
			if (HCI_TryParseContractTextureIdAndRole(AssetName, Id, Role))
			{
				FHCIContractGroup& Group = GroupsById.FindOrAdd(Id);
				Group.Id = Id;
				if (Role == TEXT("BC"))
				{
					Group.TextureBCObjectPath = Path;
				}
				else if (Role == TEXT("N"))
				{
					Group.TextureNObjectPath = Path;
				}
				else if (Role == TEXT("ORM"))
				{
					Group.TextureORMObjectPath = Path;
				}
				continue;
			}

			// Not matching strict contract (common in real projects): treat as orphan so artists can locate & fix.
			if (!Path.IsEmpty())
			{
				OrphanAssets.Add(Path);
				if (!OrphanReasonByPath.Contains(Path))
				{
					OrphanReasonByPath.Add(Path, HCI_BuildStrictContractOrphanReason(AssetName));
				}
			}
		}

		// Textures that look contract-ish but have no mesh partner are also orphans in practice.
		for (const TPair<FString, FHCIContractGroup>& Pair : GroupsById)
		{
			const FHCIContractGroup& Group = Pair.Value;
			if (!Group.MeshObjectPath.IsEmpty())
			{
				continue;
			}
			if (!Group.TextureBCObjectPath.IsEmpty())
			{
				OrphanAssets.AddUnique(Group.TextureBCObjectPath);
				OrphanReasonByPath.FindOrAdd(Group.TextureBCObjectPath) = FString::Printf(TEXT("贴图疑似契约（ID=%s），但缺少配对 Mesh：SM_%s"), *Group.Id, *Group.Id);
			}
			if (!Group.TextureNObjectPath.IsEmpty())
			{
				OrphanAssets.AddUnique(Group.TextureNObjectPath);
				OrphanReasonByPath.FindOrAdd(Group.TextureNObjectPath) = FString::Printf(TEXT("贴图疑似契约（ID=%s），但缺少配对 Mesh：SM_%s"), *Group.Id, *Group.Id);
			}
			if (!Group.TextureORMObjectPath.IsEmpty())
			{
				OrphanAssets.AddUnique(Group.TextureORMObjectPath);
				OrphanReasonByPath.FindOrAdd(Group.TextureORMObjectPath) = FString::Printf(TEXT("贴图疑似契约（ID=%s），但缺少配对 Mesh：SM_%s"), *Group.Id, *Group.Id);
			}
		}

		TArray<FHCIContractGroup> ReadyGroups;
		ReadyGroups.Reserve(GroupsById.Num());
		for (const TPair<FString, FHCIContractGroup>& Pair : GroupsById)
		{
			const FHCIContractGroup& Group = Pair.Value;
			if (Group.MeshObjectPath.IsEmpty())
			{
				continue;
			}
			if (Group.TextureBCObjectPath.IsEmpty() || Group.TextureNObjectPath.IsEmpty() || Group.TextureORMObjectPath.IsEmpty())
			{
				TArray<FString> MissingRoles;
				if (Group.TextureBCObjectPath.IsEmpty()) { MissingRoles.Add(TEXT("BC")); }
				if (Group.TextureNObjectPath.IsEmpty()) { MissingRoles.Add(TEXT("N")); }
				if (Group.TextureORMObjectPath.IsEmpty()) { MissingRoles.Add(TEXT("ORM")); }
				const FString MissingLabel = MissingRoles.Num() > 0 ? FString::Join(MissingRoles, TEXT("/")) : TEXT("BC/N/ORM");

				// Partial match: we found a mesh (and maybe some textures), but cannot complete a full PBR set.
				UnresolvedAssetSet.Add(Group.MeshObjectPath);
				UnresolvedReasonByPath.FindOrAdd(Group.MeshObjectPath) = FString::Printf(TEXT("Mesh ID=%s 缺少贴图：T_%s_%s"), *Group.Id, *Group.Id, *MissingLabel);
				if (!Group.TextureBCObjectPath.IsEmpty())
				{
					UnresolvedAssetSet.Add(Group.TextureBCObjectPath);
					UnresolvedReasonByPath.FindOrAdd(Group.TextureBCObjectPath) = FString::Printf(TEXT("该组 ID=%s 缺少贴图：%s"), *Group.Id, *MissingLabel);
				}
				if (!Group.TextureNObjectPath.IsEmpty())
				{
					UnresolvedAssetSet.Add(Group.TextureNObjectPath);
					UnresolvedReasonByPath.FindOrAdd(Group.TextureNObjectPath) = FString::Printf(TEXT("该组 ID=%s 缺少贴图：%s"), *Group.Id, *MissingLabel);
				}
				if (!Group.TextureORMObjectPath.IsEmpty())
				{
					UnresolvedAssetSet.Add(Group.TextureORMObjectPath);
					UnresolvedReasonByPath.FindOrAdd(Group.TextureORMObjectPath) = FString::Printf(TEXT("该组 ID=%s 缺少贴图：%s"), *Group.Id, *MissingLabel);
				}

				MissingGroups.Add(FString::Printf(TEXT("%s (missing_textures)"), *Group.Id));
				continue;
			}
			ReadyGroups.Add(Group);
		}

		TArray<FString> UnresolvedAssets;
		UnresolvedAssets.Reserve(UnresolvedAssetSet.Num());
		for (const FString& P : UnresolvedAssetSet)
		{
			UnresolvedAssets.Add(P);
		}
		UnresolvedAssets.Sort([](const FString& A, const FString& B) { return A < B; });
		OrphanAssets.Sort([](const FString& A, const FString& B) { return A < B; });
		for (int32 Index = OrphanAssets.Num() - 1; Index > 0; --Index)
		{
			if (OrphanAssets[Index].Equals(OrphanAssets[Index - 1], ESearchCase::CaseSensitive))
			{
				OrphanAssets.RemoveAt(Index, 1, false);
			}
		}
		for (int32 Index = UnresolvedAssets.Num() - 1; Index > 0; --Index)
		{
			if (UnresolvedAssets[Index].Equals(UnresolvedAssets[Index - 1], ESearchCase::CaseSensitive))
			{
				UnresolvedAssets.RemoveAt(Index, 1, false);
			}
		}

		if (ReadyGroups.Num() <= 0)
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4403");
			OutResult.Reason = TEXT("material_setup_no_contract_groups_found");
			OutResult.Evidence.Add(TEXT("target_root"), TargetRoot);
			OutResult.Evidence.Add(TEXT("master_material_path"), MasterMaterialPath);
			OutResult.Evidence.Add(TEXT("group_count"), TEXT("0"));
			if (MissingGroups.Num() > 0)
			{
				OutResult.Evidence.Add(TEXT("missing_groups"), FString::Join(MissingGroups, TEXT(" | ")));
			}
			OutResult.Evidence.Add(TEXT("orphan_assets"), OrphanAssets.Num() > 0 ? FString::Join(OrphanAssets, TEXT(" | ")) : TEXT("none"));
			OutResult.Evidence.Add(TEXT("unresolved_assets"), UnresolvedAssets.Num() > 0 ? FString::Join(UnresolvedAssets, TEXT(" | ")) : TEXT("none"));
			OutResult.Evidence.Add(TEXT("orphan_asset_reasons"), HCI_BuildReasonEvidenceString(OrphanReasonByPath));
			OutResult.Evidence.Add(TEXT("unresolved_asset_reasons"), HCI_BuildReasonEvidenceString(UnresolvedReasonByPath));
			OutResult.Evidence.Add(TEXT("result"), OutResult.Reason);
			return false;
		}

		// Proposals.
		const FString MaterialsDir = FString::Printf(TEXT("%s/Materials"), *TargetRoot);
		TArray<FString> ProposedInstances;
		TArray<FString> ProposedAssignments;
		TArray<FString> ProposedBindings;

		for (const FHCIContractGroup& Group : ReadyGroups)
		{
			const FString MiName = FString::Printf(TEXT("MI_%s"), *Group.Id);
			const FString MiPackagePath = FString::Printf(TEXT("%s/%s"), *MaterialsDir, *MiName);
			const FString MiObjectPath = HCI_ToObjectPath(MiPackagePath, MiName);
			ProposedInstances.Add(FString::Printf(TEXT("%s -> %s"), *MiName, *MiObjectPath));

			const FString MeshName = HCI_ExtractAssetNameFromObjectOrAssetPath(Group.MeshObjectPath);
			ProposedAssignments.Add(FString::Printf(TEXT("%s (Slot0) -> %s"), *MeshName, *MiName));

			for (const FName& Param : BaseColorParams)
			{
				ProposedBindings.Add(FString::Printf(TEXT("%s.%s <- %s"), *MiName, *Param.ToString(), *HCI_ExtractAssetNameFromObjectOrAssetPath(Group.TextureBCObjectPath)));
			}
			for (const FName& Param : NormalParams)
			{
				ProposedBindings.Add(FString::Printf(TEXT("%s.%s <- %s"), *MiName, *Param.ToString(), *HCI_ExtractAssetNameFromObjectOrAssetPath(Group.TextureNObjectPath)));
			}
			for (const FName& Param : OrmParams)
			{
				ProposedBindings.Add(FString::Printf(TEXT("%s.%s <- %s"), *MiName, *Param.ToString(), *HCI_ExtractAssetNameFromObjectOrAssetPath(Group.TextureORMObjectPath)));
			}
		}

		if (bIsDryRun)
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = true;
			OutResult.Reason = TEXT("auto_material_setup_dry_run_ok");
			OutResult.EstimatedAffectedCount = ReadyGroups.Num();
			OutResult.Evidence.Add(TEXT("target_root"), TargetRoot);
			OutResult.Evidence.Add(TEXT("master_material_path"), MasterMaterialPath);
			OutResult.Evidence.Add(TEXT("group_count"), FString::FromInt(ReadyGroups.Num()));
			OutResult.Evidence.Add(TEXT("proposed_material_instances"), ProposedInstances.Num() > 0 ? FString::Join(ProposedInstances, TEXT(" | ")) : TEXT("none"));
			OutResult.Evidence.Add(TEXT("proposed_mesh_assignments"), ProposedAssignments.Num() > 0 ? FString::Join(ProposedAssignments, TEXT(" | ")) : TEXT("none"));
			OutResult.Evidence.Add(TEXT("proposed_parameter_bindings"), ProposedBindings.Num() > 0 ? FString::Join(ProposedBindings, TEXT(" | ")) : TEXT("none"));
			OutResult.Evidence.Add(TEXT("missing_groups"), MissingGroups.Num() > 0 ? FString::Join(MissingGroups, TEXT(" | ")) : TEXT("none"));
			OutResult.Evidence.Add(TEXT("orphan_assets"), OrphanAssets.Num() > 0 ? FString::Join(OrphanAssets, TEXT(" | ")) : TEXT("none"));
			OutResult.Evidence.Add(TEXT("unresolved_assets"), UnresolvedAssets.Num() > 0 ? FString::Join(UnresolvedAssets, TEXT(" | ")) : TEXT("none"));
			OutResult.Evidence.Add(TEXT("orphan_asset_reasons"), HCI_BuildReasonEvidenceString(OrphanReasonByPath));
			OutResult.Evidence.Add(TEXT("unresolved_asset_reasons"), HCI_BuildReasonEvidenceString(UnresolvedReasonByPath));
			OutResult.Evidence.Add(TEXT("result"), OutResult.Reason);
			return true;
		}

		// Execute.
		struct FHCIMaterialSetupProgressNotification
		{
			TSharedPtr<SNotificationItem> Item;

			void Start(const FString& Text)
			{
				FNotificationInfo Info(FText::FromString(Text));
				Info.bFireAndForget = false;
				Info.FadeOutDuration = 0.2f;
				Info.ExpireDuration = 0.0f;
				Info.bUseThrobber = true;
				Info.bUseSuccessFailIcons = true;
				Item = FSlateNotificationManager::Get().AddNotification(Info);
				if (Item.IsValid())
				{
					Item->SetCompletionState(SNotificationItem::CS_Pending);
				}
			}

			void Update(const FString& Text)
			{
				if (Item.IsValid())
				{
					Item->SetText(FText::FromString(Text));
				}
			}

			void Finish(const bool bSucceeded, const FString& Text)
			{
				if (Item.IsValid())
				{
					Item->SetText(FText::FromString(Text));
					Item->SetCompletionState(bSucceeded ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
					Item->ExpireAndFadeout();
				}
			}
		};

		FHCIMaterialSetupProgressNotification Notify;
		Notify.Start(FString::Printf(TEXT("Auto-Material：准备中... (groups=%d)"), ReadyGroups.Num()));

		// Preflight: ensure destination doesn't already exist, and required assets load as expected.
		for (const FHCIContractGroup& Group : ReadyGroups)
		{
			const FString MiName = FString::Printf(TEXT("MI_%s"), *Group.Id);
			const FString MiAssetPath = FString::Printf(TEXT("%s/Materials/%s"), *TargetRoot, *MiName);
			if (UEditorAssetLibrary::DoesAssetExist(MiAssetPath))
			{
				Notify.Finish(false, TEXT("完成：Auto-Material（失败）"));
				OutResult = FHCIAbilityKitAgentToolActionResult();
				OutResult.bSucceeded = false;
				OutResult.ErrorCode = TEXT("E4404");
				OutResult.Reason = TEXT("material_instance_already_exists");
				OutResult.Evidence.Add(TEXT("target_root"), TargetRoot);
				OutResult.Evidence.Add(TEXT("master_material_path"), MasterMaterialPath);
				OutResult.Evidence.Add(TEXT("group_count"), FString::FromInt(ReadyGroups.Num()));
				OutResult.Evidence.Add(TEXT("missing_groups"), FString::Join(MissingGroups, TEXT(" | ")));
				OutResult.Evidence.Add(TEXT("orphan_assets"), OrphanAssets.Num() > 0 ? FString::Join(OrphanAssets, TEXT(" | ")) : TEXT("none"));
				OutResult.Evidence.Add(TEXT("unresolved_assets"), UnresolvedAssets.Num() > 0 ? FString::Join(UnresolvedAssets, TEXT(" | ")) : TEXT("none"));
				OutResult.Evidence.Add(TEXT("orphan_asset_reasons"), HCI_BuildReasonEvidenceString(OrphanReasonByPath));
				OutResult.Evidence.Add(TEXT("unresolved_asset_reasons"), HCI_BuildReasonEvidenceString(UnresolvedReasonByPath));
				OutResult.Evidence.Add(TEXT("result"), OutResult.Reason);
				return false;
			}

			if (!Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(Group.MeshObjectPath)) ||
				!Cast<UTexture2D>(UEditorAssetLibrary::LoadAsset(Group.TextureBCObjectPath)) ||
				!Cast<UTexture2D>(UEditorAssetLibrary::LoadAsset(Group.TextureNObjectPath)) ||
				!Cast<UTexture2D>(UEditorAssetLibrary::LoadAsset(Group.TextureORMObjectPath)))
			{
				Notify.Finish(false, TEXT("完成：Auto-Material（失败）"));
				OutResult = FHCIAbilityKitAgentToolActionResult();
				OutResult.bSucceeded = false;
				OutResult.ErrorCode = TEXT("E4405");
				OutResult.Reason = TEXT("required_assets_load_failed");
				OutResult.Evidence.Add(TEXT("target_root"), TargetRoot);
				OutResult.Evidence.Add(TEXT("master_material_path"), MasterMaterialPath);
				OutResult.Evidence.Add(TEXT("group_count"), FString::FromInt(ReadyGroups.Num()));
				OutResult.Evidence.Add(TEXT("orphan_assets"), OrphanAssets.Num() > 0 ? FString::Join(OrphanAssets, TEXT(" | ")) : TEXT("none"));
				OutResult.Evidence.Add(TEXT("unresolved_assets"), UnresolvedAssets.Num() > 0 ? FString::Join(UnresolvedAssets, TEXT(" | ")) : TEXT("none"));
				OutResult.Evidence.Add(TEXT("orphan_asset_reasons"), HCI_BuildReasonEvidenceString(OrphanReasonByPath));
				OutResult.Evidence.Add(TEXT("unresolved_asset_reasons"), HCI_BuildReasonEvidenceString(UnresolvedReasonByPath));
				OutResult.Evidence.Add(TEXT("result"), OutResult.Reason);
				return false;
			}
		}

		UEditorAssetLibrary::MakeDirectory(MaterialsDir);

		TArray<FString> CreatedInstances;
		TArray<FString> AppliedAssignments;
		TArray<FString> FailedRows;

		int32 Completed = 0;
		for (const FHCIContractGroup& Group : ReadyGroups)
		{
			++Completed;
			Notify.Update(FString::Printf(TEXT("Auto-Material：执行中... (%d/%d)"), Completed, ReadyGroups.Num()));
			// Keep editor UI responsive during batch operations.
			FSlateApplication::Get().PumpMessages();

			const FString MiName = FString::Printf(TEXT("MI_%s"), *Group.Id);
			const FString MiDir = MaterialsDir;

			UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
			Factory->InitialParent = MasterMaterial;

			UObject* Created = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().CreateAsset(
				MiName,
				MiDir,
				UMaterialInstanceConstant::StaticClass(),
				Factory);

			UMaterialInstanceConstant* MI = Cast<UMaterialInstanceConstant>(Created);
			if (!MI)
			{
				FailedRows.Add(FString::Printf(TEXT("%s (create_mi_failed)"), *MiName));
				break;
			}

			UTexture* TexBC = Cast<UTexture>(UEditorAssetLibrary::LoadAsset(Group.TextureBCObjectPath));
			UTexture* TexN = Cast<UTexture>(UEditorAssetLibrary::LoadAsset(Group.TextureNObjectPath));
			UTexture* TexORM = Cast<UTexture>(UEditorAssetLibrary::LoadAsset(Group.TextureORMObjectPath));

			MI->Modify();
			for (const FName& Param : BaseColorParams)
			{
				MI->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(Param), TexBC);
			}
			for (const FName& Param : NormalParams)
			{
				MI->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(Param), TexN);
			}
			for (const FName& Param : OrmParams)
			{
				MI->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(Param), TexORM);
			}
			MI->PostEditChange();
			UEditorAssetLibrary::SaveLoadedAsset(MI, false);

			CreatedInstances.Add(MI->GetPathName());

			UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(Group.MeshObjectPath));
			if (!Mesh)
			{
				FailedRows.Add(FString::Printf(TEXT("%s (mesh_load_failed)"), *Group.MeshObjectPath));
				break;
			}

			Mesh->Modify();
			Mesh->SetMaterial(0, MI);
			Mesh->PostEditChange();
			UEditorAssetLibrary::SaveLoadedAsset(Mesh, false);
			AppliedAssignments.Add(FString::Printf(TEXT("%s (Slot0) -> %s"), *Mesh->GetPathName(), *MI->GetPathName()));
		}

		OutResult = FHCIAbilityKitAgentToolActionResult();
		const bool bAllOk = FailedRows.Num() == 0;
		OutResult.bSucceeded = bAllOk;
		OutResult.ErrorCode = bAllOk ? FString() : TEXT("E4406");
		OutResult.Reason = bAllOk ? TEXT("auto_material_setup_execute_ok") : TEXT("auto_material_setup_execute_failed");
		OutResult.EstimatedAffectedCount = CreatedInstances.Num();
		OutResult.Evidence.Add(TEXT("target_root"), TargetRoot);
		OutResult.Evidence.Add(TEXT("master_material_path"), MasterMaterialPath);
		OutResult.Evidence.Add(TEXT("group_count"), FString::FromInt(ReadyGroups.Num()));
		OutResult.Evidence.Add(TEXT("proposed_material_instances"), ProposedInstances.Num() > 0 ? FString::Join(ProposedInstances, TEXT(" | ")) : TEXT("none"));
		OutResult.Evidence.Add(TEXT("proposed_mesh_assignments"), ProposedAssignments.Num() > 0 ? FString::Join(ProposedAssignments, TEXT(" | ")) : TEXT("none"));
		OutResult.Evidence.Add(TEXT("proposed_parameter_bindings"), ProposedBindings.Num() > 0 ? FString::Join(ProposedBindings, TEXT(" | ")) : TEXT("none"));
		OutResult.Evidence.Add(TEXT("missing_groups"), MissingGroups.Num() > 0 ? FString::Join(MissingGroups, TEXT(" | ")) : TEXT("none"));
		OutResult.Evidence.Add(TEXT("orphan_assets"), OrphanAssets.Num() > 0 ? FString::Join(OrphanAssets, TEXT(" | ")) : TEXT("none"));
		OutResult.Evidence.Add(TEXT("unresolved_assets"), UnresolvedAssets.Num() > 0 ? FString::Join(UnresolvedAssets, TEXT(" | ")) : TEXT("none"));
		OutResult.Evidence.Add(TEXT("orphan_asset_reasons"), HCI_BuildReasonEvidenceString(OrphanReasonByPath));
		OutResult.Evidence.Add(TEXT("unresolved_asset_reasons"), HCI_BuildReasonEvidenceString(UnresolvedReasonByPath));
		OutResult.Evidence.Add(TEXT("result"), OutResult.Reason);
		if (FailedRows.Num() > 0)
		{
			OutResult.Evidence.Add(TEXT("failed_assets"), FString::Join(FailedRows, TEXT(" | ")));
		}

		Notify.Finish(bAllOk, bAllOk ? TEXT("完成：Auto-Material") : TEXT("完成：Auto-Material（失败）"));
		return bAllOk;
	}
};

class FHCIAbilityKitRenameAssetToolAction final : public IHCIAbilityKitAgentToolAction
{
public:
	virtual FName GetToolName() const override
	{
		return TEXT("RenameAsset");
	}

	virtual bool DryRun(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		FString SourceAssetPath;
		FString NewName;
		if (!HCI_TryReadRequiredStringArg(Request.Args, TEXT("asset_path"), SourceAssetPath) ||
			!HCI_TryReadRequiredStringArg(Request.Args, TEXT("new_name"), NewName))
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4001");
			OutResult.Reason = TEXT("required_arg_missing");
			return false;
		}

		OutResult = FHCIAbilityKitAgentToolActionResult();
		if (!HCI_ValidateSourceAssetExists(SourceAssetPath, OutResult))
		{
			return false;
		}

		FString SourcePackagePath;
		FString SourceAssetName;
		if (!HCI_TrySplitObjectPath(SourceAssetPath, SourcePackagePath, SourceAssetName))
		{
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4009");
			OutResult.Reason = TEXT("asset_path_invalid");
			return false;
		}

		const FString SourceDir = HCI_GetDirectoryFromPackagePath(SourcePackagePath);
		const FString DestinationPackagePath = FString::Printf(TEXT("%s/%s"), *SourceDir, *NewName);
		const FString DestinationAssetPath = HCI_ToObjectPath(DestinationPackagePath, NewName);

		OutResult.bSucceeded = true;
		OutResult.Reason = TEXT("rename_dry_run_ok");
		OutResult.EstimatedAffectedCount = 1;
		OutResult.Evidence.Add(TEXT("asset_path"), SourceAssetPath);
		OutResult.Evidence.Add(TEXT("before"), SourceAssetPath);
		OutResult.Evidence.Add(TEXT("after"), DestinationAssetPath);
		OutResult.Evidence.Add(TEXT("result"), TEXT("rename_dry_run_ok"));
		return true;
	}

	virtual bool Execute(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		if (!DryRun(Request, OutResult))
		{
			return false;
		}

		const FString SourceAssetPath = OutResult.Evidence.FindRef(TEXT("before"));
		const FString DestinationAssetPath = OutResult.Evidence.FindRef(TEXT("after"));
		if (SourceAssetPath.IsEmpty() || DestinationAssetPath.IsEmpty())
		{
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4303");
			OutResult.Reason = TEXT("rename_execute_missing_paths");
			return false;
		}

		if (SourceAssetPath.Equals(DestinationAssetPath, ESearchCase::CaseSensitive))
		{
			OutResult.bSucceeded = true;
			OutResult.Reason = TEXT("rename_noop_same_path");
			OutResult.Evidence.Add(TEXT("result"), TEXT("rename_noop_same_path"));
			return true;
		}

		const int32 DotIndex = DestinationAssetPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart);
		const FString DestinationPackagePath = (DotIndex == INDEX_NONE) ? DestinationAssetPath : DestinationAssetPath.Left(DotIndex);
		const FString DestinationDir = HCI_GetDirectoryFromPackagePath(DestinationPackagePath);
		if (!DestinationDir.IsEmpty())
		{
			UEditorAssetLibrary::MakeDirectory(DestinationDir);
		}

		const bool bRenamed = UEditorAssetLibrary::RenameAsset(SourceAssetPath, DestinationAssetPath);
		OutResult.bSucceeded = bRenamed;
		OutResult.ErrorCode = bRenamed ? FString() : TEXT("E4203");
		OutResult.Reason = bRenamed ? TEXT("rename_execute_ok") : TEXT("rename_execute_failed");
		if (bRenamed)
		{
			HCI_FixupRedirectorReferencers(SourceAssetPath, OutResult);
		}
		OutResult.Evidence.Add(TEXT("result"), bRenamed ? TEXT("rename_execute_ok") : TEXT("rename_execute_failed"));
		return bRenamed;
	}
};

class FHCIAbilityKitMoveAssetToolAction final : public IHCIAbilityKitAgentToolAction
{
public:
	virtual FName GetToolName() const override
	{
		return TEXT("MoveAsset");
	}

	virtual bool DryRun(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		FString SourceAssetPath;
		FString TargetPath;
		if (!HCI_TryReadRequiredStringArg(Request.Args, TEXT("asset_path"), SourceAssetPath) ||
			!HCI_TryReadRequiredStringArg(Request.Args, TEXT("target_path"), TargetPath))
		{
			OutResult = FHCIAbilityKitAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4001");
			OutResult.Reason = TEXT("required_arg_missing");
			return false;
		}

		OutResult = FHCIAbilityKitAgentToolActionResult();
		if (!HCI_ValidateSourceAssetExists(SourceAssetPath, OutResult))
		{
			return false;
		}

		FString SourcePackagePath;
		FString SourceAssetName;
		if (!HCI_TrySplitObjectPath(SourceAssetPath, SourcePackagePath, SourceAssetName))
		{
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4009");
			OutResult.Reason = TEXT("asset_path_invalid");
			return false;
		}

		FString DestinationAssetPath;
		if (TargetPath.Contains(TEXT(".")))
		{
			DestinationAssetPath = TargetPath;
		}
		else
		{
			FString TargetDir = TargetPath;
			if (TargetDir.EndsWith(TEXT("/")))
			{
				TargetDir.LeftChopInline(1, false);
			}
			DestinationAssetPath = FString::Printf(TEXT("%s/%s.%s"), *TargetDir, *SourceAssetName, *SourceAssetName);
		}

		if (!DestinationAssetPath.StartsWith(TEXT("/Game/")))
		{
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4009");
			OutResult.Reason = TEXT("target_path_must_start_with_game");
			return false;
		}

		OutResult.bSucceeded = true;
		OutResult.Reason = TEXT("move_dry_run_ok");
		OutResult.EstimatedAffectedCount = 1;
		OutResult.Evidence.Add(TEXT("asset_path"), SourceAssetPath);
		OutResult.Evidence.Add(TEXT("before"), SourceAssetPath);
		OutResult.Evidence.Add(TEXT("after"), DestinationAssetPath);
		OutResult.Evidence.Add(TEXT("result"), TEXT("move_dry_run_ok"));
		return true;
	}

	virtual bool Execute(
		const FHCIAbilityKitAgentToolActionRequest& Request,
		FHCIAbilityKitAgentToolActionResult& OutResult) const override
	{
		if (!DryRun(Request, OutResult))
		{
			return false;
		}

		const FString SourceAssetPath = OutResult.Evidence.FindRef(TEXT("before"));
		const FString DestinationAssetPath = OutResult.Evidence.FindRef(TEXT("after"));
		if (SourceAssetPath.IsEmpty() || DestinationAssetPath.IsEmpty())
		{
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4303");
			OutResult.Reason = TEXT("move_execute_missing_paths");
			return false;
		}

		const int32 DotIndex = DestinationAssetPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart);
		const FString DestinationPackagePath = (DotIndex == INDEX_NONE) ? DestinationAssetPath : DestinationAssetPath.Left(DotIndex);
		const FString DestinationDir = HCI_GetDirectoryFromPackagePath(DestinationPackagePath);
		if (!DestinationDir.IsEmpty())
		{
			UEditorAssetLibrary::MakeDirectory(DestinationDir);
		}

		const bool bMoved = HCI_MoveAssetWithAssetTools(SourceAssetPath, DestinationAssetPath);
		OutResult.bSucceeded = bMoved;
		OutResult.ErrorCode = bMoved ? FString() : TEXT("E4204");
		OutResult.Reason = bMoved ? TEXT("move_execute_ok") : TEXT("move_execute_failed");
		if (bMoved)
		{
			HCI_FixupRedirectorReferencers(SourceAssetPath, OutResult);
		}
		OutResult.Evidence.Add(TEXT("result"), bMoved ? TEXT("move_execute_ok") : TEXT("move_execute_failed"));
		return bMoved;
	}
};
#endif
}

void HCIAbilityKitAgentToolActions::BuildStageIDraftActions(TMap<FName, TSharedPtr<IHCIAbilityKitAgentToolAction>>& OutActions)
{
	OutActions.Reset();
	OutActions.Add(TEXT("ScanAssets"), HCIAbilityKitToolActionFactories::MakeScanAssetsToolAction());
	OutActions.Add(TEXT("ScanMeshTriangleCount"), HCIAbilityKitToolActionFactories::MakeScanMeshTriangleCountToolAction());
	OutActions.Add(TEXT("SearchPath"), HCIAbilityKitToolActionFactories::MakeSearchPathToolAction());
	OutActions.Add(TEXT("ScanLevelMeshRisks"), HCIAbilityKitToolActionFactories::MakeScanLevelMeshRisksToolAction());
	OutActions.Add(TEXT("SetTextureMaxSize"), HCIAbilityKitToolActionFactories::MakeSetTextureMaxSizeToolAction());
	OutActions.Add(TEXT("SetMeshLODGroup"), HCIAbilityKitToolActionFactories::MakeSetMeshLODGroupToolAction());
	OutActions.Add(TEXT("NormalizeAssetNamingByMetadata"), HCIAbilityKitToolActionFactories::MakeNormalizeAssetNamingByMetadataToolAction());
	OutActions.Add(TEXT("AutoMaterialSetupByNameContract"), HCIAbilityKitToolActionFactories::MakeAutoMaterialSetupByNameContractToolAction());
	OutActions.Add(TEXT("RenameAsset"), HCIAbilityKitToolActionFactories::MakeRenameAssetToolAction());
	OutActions.Add(TEXT("MoveAsset"), HCIAbilityKitToolActionFactories::MakeMoveAssetToolAction());
}
