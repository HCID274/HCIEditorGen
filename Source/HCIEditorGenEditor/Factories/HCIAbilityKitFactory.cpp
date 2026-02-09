#include "HCIAbilityKitFactory.h"

#include "HCIEditorGen/Public/HCIAbilityKitAsset.h" // 引用你的资产定义
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

	const FString FullFilename = Files[0];
	
	if (!FPaths::FileExists(FullFilename))
	{
		UE_LOG(LogHCIAbilityKitFactory, Error, TEXT("Reimport failed: File not found: %s"), *FullFilename);
		return EReimportResult::Failed;
	}

	// 2. 重新解析
	FParsedKit ParsedData;
	FString ErrorMsg;
	if (!TryParseKitFile(FullFilename, ParsedData, ErrorMsg))
	{
		UE_LOG(LogHCIAbilityKitFactory, Error, TEXT("Reimport failed: %s"), *ErrorMsg);
		return EReimportResult::Failed;
	}

	// 3. 写入数据（关键：Modify 用于支持撤销/Undo，MarkPackageDirty 用于提示保存）
	MyAsset->Modify();
	ApplyParsedToAsset(MyAsset, ParsedData);
#if WITH_EDITORONLY_DATA
	if (MyAsset->AssetImportData)
	{
		MyAsset->AssetImportData->Update(FPaths::ConvertRelativePathToFull(FullFilename));
	}
#endif
	MyAsset->MarkPackageDirty();

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

	// 3. 提取字段 (这里对应你 DataAsset 里的属性)
	// 如果 JSON 里没有这些字段，我们就用默认值
	OutParsed.SchemaVersion = RootObject->GetIntegerField(TEXT("schema_version"));
	OutParsed.Id = RootObject->GetStringField(TEXT("id"));
	OutParsed.DisplayName = RootObject->GetStringField(TEXT("display_name"));
	
	// 读取嵌套的对象 (Params -> Damage)
	const TSharedPtr<FJsonObject>* ParamsObj = nullptr;
	if (RootObject->TryGetObjectField(TEXT("params"), ParamsObj))
	{
		OutParsed.Damage = (*ParamsObj)->GetNumberField(TEXT("damage"));
	}
	else
	{
		OutParsed.Damage = 0.0f;
	}

	// 记录原始路径用于重导入
	OutParsed.SourceFileToStore = MakeSourcePathToStore(FullFilename);

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
	Asset->SourceFile = Parsed.SourceFileToStore;
#endif
}

FString UHCIAbilityKitFactory::MakeSourcePathToStore(const FString& FullFilename)
{
	// 尽量存相对路径（相对于项目目录），这样队友拉下来也能用
	FString RelPath = FullFilename;
	FPaths::MakePathRelativeTo(RelPath, *FPaths::ProjectDir());
	return RelPath;
}

FString UHCIAbilityKitFactory::ResolveSourcePath(const FString& StoredPath)
{
	// 把相对路径还原成绝对路径
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), StoredPath);
}