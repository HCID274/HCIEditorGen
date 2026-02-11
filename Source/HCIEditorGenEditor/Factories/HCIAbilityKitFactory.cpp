#include "HCIAbilityKitFactory.h"

#include "HCIEditorGen/Public/HCIAbilityKitAsset.h" // 引用你的资产定义
#include "EditorFramework/AssetImportData.h" // <--- 本喵把它加在这里啦喵！
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/FeedbackContext.h" // 用于 Warn->Logf

// JSON 处理相关
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// 定义日志分类，方便调试
DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitFactory, Log, All);

UHCIAbilityKitFactory::UHCIAbilityKitFactory()
{
	// 1. 告诉编辑器我支持什么后缀名
	// 格式：后缀;描述
	Formats.Add(TEXT("hciabilitykit;HCI Ability Kit (*.hciabilitykit)"));

	// 2. 告诉编辑器我产出什么类型的资产
	SupportedClass = UHCIAbilityKitAsset::StaticClass();

	// 3. 配置工厂属性
	bCreateNew = false; // 不允许在该类上点右键创建（必须从文件导入）
	bText = true;       // 这是一个文本文件工厂（非二进制）
	bEditorImport = true; // 这是一个编辑器导入工厂
}

bool UHCIAbilityKitFactory::FactoryCanImport(const FString& Filename)
{
	// 简单粗暴：只看后缀名是不是 .hciabilitykit
	return FPaths::GetExtension(Filename, true).Equals(TEXT(".hciabilitykit"), ESearchCase::IgnoreCase);
}

// =========================================================
// 核心：创建新资产 (Import)
// =========================================================
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

	// 1. 解析文件内容到临时结构体
	FParsedKit ParsedData;
	FString ErrorMsg;
	
	// 注意：这里调用的是我们自己在下面写的私有函数
	if (!TryParseKitFile(Filename, ParsedData, ErrorMsg))
	{
		if (Warn)
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("HCIAbilityKit import failed: %s"), *ErrorMsg);
		}
		// 也要输出到 Output Log 面板
		UE_LOG(LogHCIAbilityKitFactory, Error, TEXT("Import failed: %s"), *ErrorMsg);
		
		bOutOperationCanceled = true;
		return nullptr;
	}

	// 2. 创建资产对象 (NewObject)
	UHCIAbilityKitAsset* NewAsset = NewObject<UHCIAbilityKitAsset>(InParent, InClass, InName, Flags);
	if (!NewAsset)
	{
		bOutOperationCanceled = true;
		return nullptr;
	}

	// 3. 把解析好的数据填进去
	ApplyParsedToAsset(NewAsset, ParsedData);

#if WITH_EDITORONLY_DATA
	if (NewAsset->AssetImportData)
	{
		const FString AbsoluteFilename = FPaths::ConvertRelativePathToFull(Filename);
		NewAsset->AssetImportData->Update(AbsoluteFilename);
	}
#endif

	NewAsset->MarkPackageDirty();
	return NewAsset;
}

// =========================================================
// 核心：重导入 (Reimport)
// =========================================================

bool UHCIAbilityKitFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UHCIAbilityKitAsset* MyAsset = Cast<UHCIAbilityKitAsset>(Obj);
	if (!MyAsset)
	{
		return false;
	}

#if WITH_EDITORONLY_DATA
	if (MyAsset->AssetImportData)
	{
		MyAsset->AssetImportData->ExtractFilenames(OutFilenames);
		return OutFilenames.Num() > 0;
	}
#endif
	return false;
}

void UHCIAbilityKitFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UHCIAbilityKitAsset* MyAsset = Cast<UHCIAbilityKitAsset>(Obj);
	if (MyAsset && NewReimportPaths.Num() > 0)
	{
#if WITH_EDITORONLY_DATA
		// 更新内部存储的路径（存相对路径，方便多人协作）
		if (MyAsset->AssetImportData)
		{
			MyAsset->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
		}
#endif
	}
}

EReimportResult::Type UHCIAbilityKitFactory::Reimport(UObject* Obj)
{
	UHCIAbilityKitAsset* MyAsset = Cast<UHCIAbilityKitAsset>(Obj);
	if (!MyAsset)
	{
		return EReimportResult::Failed;
	}

#if WITH_EDITORONLY_DATA
	// 1. 找回文件路径
	TArray<FString> Files;
	if (!MyAsset->AssetImportData) return EReimportResult::Failed;

	MyAsset->AssetImportData->ExtractFilenames(Files);
	if (Files.Num()<1) return EReimportResult::Failed;

	// **新增**：处理相对路径，确保路径是绝对路径
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

	// 2. 重新解析
	FParsedKit ParsedData;
	FString ErrorMsg;
	if (!TryParseKitFile(FilenameToLoad, ParsedData, ErrorMsg))
	{
		UE_LOG(LogHCIAbilityKitFactory, Error, TEXT("Reimport failed: %s"), *ErrorMsg);
		return EReimportResult::Failed;
	}

	// 3. 写入数据（关键：Modify 用于支持撤销/Undo，MarkPackageDirty 用于提示保存）
	MyAsset->Modify();
	ApplyParsedToAsset(MyAsset, ParsedData);
	
	if (MyAsset->AssetImportData)
	{
		MyAsset->AssetImportData->Update(FilenameToLoad);
	}
	
	MyAsset->MarkPackageDirty();

	// **新增**：通知编辑器资产已变更，刷新UI
	MyAsset->PostEditChange();

	return EReimportResult::Succeeded;
#else
	return EReimportResult::Failed;
#endif
}

// =========================================================
// 辅助函数 (Helpers)
// =========================================================

bool UHCIAbilityKitFactory::TryParseKitFile(const FString& FullFilename, FParsedKit& OutParsed, FString& OutError)
{
	FString FileContent;
	// 1. 读文件到字符串
	if (!FFileHelper::LoadFileToString(FileContent, *FullFilename))
	{
		OutError = FString::Printf(TEXT("Failed to load file: %s"), *FullFilename);
		return false;
	}

	// 2. 解析 JSON
	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContent);

	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Invalid JSON format");
		return false;
	}

	// 3. 提取字段 (安全模式)
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
	
	// 读取嵌套的对象 (Params -> Damage)
	const TSharedPtr<FJsonObject>* ParamsObj = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("params"), ParamsObj) || !ParamsObj || !(*ParamsObj).IsValid())
	{
		OutError = TEXT("Missing or invalid object: params");
		return false;
	}

	double DamageTmp; // JSON 数字是 double
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
	if (!Asset) return;

	// 把解析出来的数据赋值给 Asset
	Asset->SchemaVersion = Parsed.SchemaVersion;
	Asset->Id = Parsed.Id;
	Asset->DisplayName = Parsed.DisplayName;
	Asset->Damage = Parsed.Damage;

#if WITH_EDITORONLY_DATA
	
#endif
}