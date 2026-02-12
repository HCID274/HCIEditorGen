#include "Factories/HCIAbilityKitFactory.h"

#include "HCIAbilityKitAsset.h"

#include "Dom/JsonObject.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/FeedbackContext.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitFactory, Log, All);

UHCIAbilityKitFactory::UHCIAbilityKitFactory()
{
	// 注册支持的文件扩展名和描述喵
	Formats.Add(TEXT("hciabilitykit;HCI Ability Kit (*.hciabilitykit)"));
	// 指定工厂创建的资产类型喵
	SupportedClass = UHCIAbilityKitAsset::StaticClass();
	// 设置为通过文件导入而非直接在编辑器创建喵
	bCreateNew = false;
	bText = true;
	bEditorImport = true;
}

bool UHCIAbilityKitFactory::FactoryCanImport(const FString& Filename)
{
	// 检查文件扩展名是否匹配喵
	return FPaths::GetExtension(Filename, true).Equals(TEXT(".hciabilitykit"), ESearchCase::IgnoreCase);
}

UObject* UHCIAbilityKitFactory::FactoryCreateFile(
	UClass* InClass,
	UObject* InParent,
	FName InName,
	EObjectFlags Flags,
	const FString& Filename,
	const TCHAR* Parms,
	FFeedbackContext* Warn,
	bool& bOutOperationCanceled)
{
	bOutOperationCanceled = false;

	FParsedKit ParsedData;
	FString ErrorMsg;
	// 尝试解析源文件喵
	if (!TryParseKitFile(Filename, ParsedData, ErrorMsg))
	{
		if (Warn)
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("HCIAbilityKit import failed: %s"), *ErrorMsg);
		}
		UE_LOG(LogHCIAbilityKitFactory, Error, TEXT("Import failed: %s"), *ErrorMsg);
		bOutOperationCanceled = true;
		return nullptr;
	}

	// 创建资产实例喵
	UHCIAbilityKitAsset* NewAsset = NewObject<UHCIAbilityKitAsset>(InParent, InClass, InName, Flags);
	if (!NewAsset)
	{
		bOutOperationCanceled = true;
		return nullptr;
	}

	// 将解析的数据应用到新资产上喵
	ApplyParsedToAsset(NewAsset, ParsedData);

#if WITH_EDITORONLY_DATA
	// 更新资产的导入源文件信息，用于重导入功能喵
	if (NewAsset->AssetImportData)
	{
		const FString AbsoluteFilename = FPaths::ConvertRelativePathToFull(Filename);
		NewAsset->AssetImportData->Update(AbsoluteFilename);
	}
#endif

	NewAsset->MarkPackageDirty();
	return NewAsset;
}

bool UHCIAbilityKitFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UHCIAbilityKitAsset* Asset = Cast<UHCIAbilityKitAsset>(Obj);
	if (!Asset)
	{
		return false;
	}

#if WITH_EDITORONLY_DATA
	// 提取资产中记录的源文件路径喵
	if (Asset->AssetImportData)
	{
		Asset->AssetImportData->ExtractFilenames(OutFilenames);
		return OutFilenames.Num() > 0;
	}
#endif
	return false;
}

void UHCIAbilityKitFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UHCIAbilityKitAsset* Asset = Cast<UHCIAbilityKitAsset>(Obj);
	if (!Asset || NewReimportPaths.Num() == 0)
	{
		return;
	}

#if WITH_EDITORONLY_DATA
	// 更新记录的重导入路径喵
	if (Asset->AssetImportData)
	{
		Asset->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
#endif
}

EReimportResult::Type UHCIAbilityKitFactory::Reimport(UObject* Obj)
{
	UHCIAbilityKitAsset* Asset = Cast<UHCIAbilityKitAsset>(Obj);
	if (!Asset)
	{
		return EReimportResult::Failed;
	}

#if WITH_EDITORONLY_DATA
	if (!Asset->AssetImportData)
	{
		return EReimportResult::Failed;
	}

	TArray<FString> Files;
	Asset->AssetImportData->ExtractFilenames(Files);
	if (Files.Num() < 1)
	{
		return EReimportResult::Failed;
	}

	// 转换为绝对路径以确保能正确读取喵
	FString FilenameToLoad = Files[0];
	if (FPaths::IsRelative(FilenameToLoad))
	{
		FilenameToLoad = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), FilenameToLoad);
	}

	if (!FPaths::FileExists(FilenameToLoad))
	{
		UE_LOG(LogHCIAbilityKitFactory, Error, TEXT("Reimport failed: File not found: %s"), *FilenameToLoad);
		return EReimportResult::Failed;
	}

	FParsedKit ParsedData;
	FString ErrorMsg;
	// 重新解析文件喵
	if (!TryParseKitFile(FilenameToLoad, ParsedData, ErrorMsg))
	{
		UE_LOG(LogHCIAbilityKitFactory, Error, TEXT("Reimport failed: %s"), *ErrorMsg);
		return EReimportResult::Failed;
	}

	// 标记对象即将修改，记录撤销/重做快照喵
	Asset->Modify();
	ApplyParsedToAsset(Asset, ParsedData);
	Asset->AssetImportData->Update(FilenameToLoad);
	Asset->MarkPackageDirty();
	// 通知编辑器资产已更改喵
	Asset->PostEditChange();
	return EReimportResult::Succeeded;
#else
	return EReimportResult::Failed;
#endif
}

bool UHCIAbilityKitFactory::TryParseKitFile(const FString& FullFilename, FParsedKit& OutParsed, FString& OutError)
{
	FString FileContent;
	// 读取文件内容为字符串喵
	if (!FFileHelper::LoadFileToString(FileContent, *FullFilename))
	{
		OutError = FString::Printf(TEXT("Failed to load file: %s"), *FullFilename);
		return false;
	}

	// 解析 JSON 喵
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Invalid JSON format");
		return false;
	}

	// 校验并提取各字段喵
	int32 SchemaVersionTmp;
	if (!RootObject->TryGetNumberField(TEXT("schema_version"), SchemaVersionTmp))
	{
		OutError = TEXT("Missing or invalid field: schema_version (must be an integer)");
		return false;
	}
	OutParsed.SchemaVersion = SchemaVersionTmp;

	FString IdTmp;
	if (!RootObject->TryGetStringField(TEXT("id"), IdTmp) || IdTmp.IsEmpty())
	{
		OutError = TEXT("Missing or invalid field: id (must be a non-empty string)");
		return false;
	}
	OutParsed.Id = IdTmp;

	FString DisplayNameTmp;
	if (!RootObject->TryGetStringField(TEXT("display_name"), DisplayNameTmp))
	{
		OutError = TEXT("Missing or invalid field: display_name (must be a string)");
		return false;
	}
	OutParsed.DisplayName = DisplayNameTmp;

	const TSharedPtr<FJsonObject>* ParamsObj = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("params"), ParamsObj) || !ParamsObj || !ParamsObj->IsValid())
	{
		OutError = TEXT("Missing or invalid object: params");
		return false;
	}

	double DamageTmp;
	if (!(*ParamsObj)->TryGetNumberField(TEXT("damage"), DamageTmp))
	{
		OutError = TEXT("Missing or invalid field in params: damage (must be a number)");
		return false;
	}
	OutParsed.Damage = static_cast<float>(DamageTmp);
	return true;
}

void UHCIAbilityKitFactory::ApplyParsedToAsset(UHCIAbilityKitAsset* Asset, const FParsedKit& Parsed)
{
	if (!Asset)
	{
		return;
	}

	// 将解析好的结构体数据同步到资产属性中喵
	Asset->SchemaVersion = Parsed.SchemaVersion;
	Asset->Id = Parsed.Id;
	Asset->DisplayName = Parsed.DisplayName;
	Asset->Damage = Parsed.Damage;
}
