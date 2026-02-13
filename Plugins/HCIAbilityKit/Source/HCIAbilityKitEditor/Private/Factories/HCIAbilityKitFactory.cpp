#include "Factories/HCIAbilityKitFactory.h"

#include "HCIAbilityKitAsset.h"
#include "Services/HCIAbilityKitParserService.h"

#include "EditorFramework/AssetImportData.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/FeedbackContext.h"
#include "Misc/Paths.h"
#include "Search/HCIAbilityKitSearchIndexService.h"
#include "Widgets/Notifications/SNotificationList.h"

// 静态日志分类定义
DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitFactory, Log, All);

namespace
{
/**
 * 在虚幻编辑器中显示导入/重导入失败的桌面通知
 */
void ShowReimportFailureNotification(const UObject* Asset, const FString& Reason)
{
#if WITH_EDITOR
	const FString AssetLabel = Asset ? Asset->GetName() : TEXT("<InvalidAsset>");
	const FText Message = FText::FromString(FString::Printf(TEXT("Reimport failed for %s: %s"), *AssetLabel, *Reason));

	FNotificationInfo NotificationInfo(Message);
	NotificationInfo.bFireAndForget = true;
	NotificationInfo.ExpireDuration = 6.0f;
	NotificationInfo.FadeOutDuration = 0.2f;
	NotificationInfo.bUseSuccessFailIcons = true;

	const TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(NotificationInfo);
	if (Notification.IsValid())
	{
		Notification->SetCompletionState(SNotificationItem::CS_Fail);
	}
#else
	(void)Asset;
	(void)Reason;
#endif
}
}

UHCIAbilityKitFactory::UHCIAbilityKitFactory()
{
	// 注册受支持的文件扩展名及其 UI 描述字符串
	Formats.Add(TEXT("hciabilitykit;HCI Ability Kit (*.hciabilitykit)"));
	// 关联该工厂创建的资产类
	SupportedClass = UHCIAbilityKitAsset::StaticClass();
	// 设置该工厂仅支持通过文件导入，不支持在内容浏览器中直接新建
	bCreateNew = false;
	bText = true;
	bEditorImport = true;
}

bool UHCIAbilityKitFactory::FactoryCanImport(const FString& Filename)
{
	// 基于文件扩展名判断是否可以进行导入
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

	FHCIAbilityKitParsedData ParsedData;
	FHCIAbilityKitParseError ParseError;
	
	// 调用解析服务解析源文件内容
	if (!FHCIAbilityKitParserService::TryParseKitFile(Filename, ParsedData, ParseError))
	{
		const FString ErrorMsg = ParseError.ToContractString();
		if (Warn)
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("HCIAbilityKit import failed: %s"), *ErrorMsg);
		}
		UE_LOG(LogHCIAbilityKitFactory, Error, TEXT("Import failed: %s"), *ErrorMsg);
		
		if (!ParseError.Detail.IsEmpty())
		{
			UE_LOG(LogHCIAbilityKitFactory, Error, TEXT("Import detail: %s"), *ParseError.Detail);
		}
		
		bOutOperationCanceled = true;
		return nullptr;
	}

	// 成功解析后创建新的资产对象
	UHCIAbilityKitAsset* NewAsset = NewObject<UHCIAbilityKitAsset>(InParent, InClass, InName, Flags);
	if (!NewAsset)
	{
		bOutOperationCanceled = true;
		return nullptr;
	}

	// 将解析结果应用到资产属性中
	ApplyParsedToAsset(NewAsset, ParsedData);

#if WITH_EDITORONLY_DATA
	// 记录源文件路径，使资产支持重导入流程
	if (NewAsset->AssetImportData)
	{
		const FString AbsoluteFilename = FPaths::ConvertRelativePathToFull(Filename);
		NewAsset->AssetImportData->Update(AbsoluteFilename);
	}
#endif

	NewAsset->MarkPackageDirty();
	FHCIAbilityKitSearchIndexService::Get().RefreshAsset(NewAsset);
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
	// 尝试从资产关联的导入数据中提取源文件路径
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
	// 更新资产记录的源文件重导入路径
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
		ShowReimportFailureNotification(Obj, TEXT("Selected object is not UHCIAbilityKitAsset"));
		return EReimportResult::Failed;
	}

#if WITH_EDITORONLY_DATA
	if (!Asset->AssetImportData)
	{
		ShowReimportFailureNotification(Asset, TEXT("AssetImportData is missing"));
		return EReimportResult::Failed;
	}

	TArray<FString> Files;
	Asset->AssetImportData->ExtractFilenames(Files);
	if (Files.Num() < 1)
	{
		ShowReimportFailureNotification(Asset, TEXT("No recorded source file path"));
		return EReimportResult::Failed;
	}

	// 转换为绝对路径以确保文件系统能够正确定位
	FString FilenameToLoad = Files[0];
	if (FPaths::IsRelative(FilenameToLoad))
	{
		FilenameToLoad = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), FilenameToLoad);
	}

	// 验证源文件是否存在
	if (!FPaths::FileExists(FilenameToLoad))
	{
		FHCIAbilityKitParseError ParseError;
		ParseError.Code = TEXT("E1005");
		ParseError.File = FilenameToLoad;
		ParseError.Field = TEXT("file");
		ParseError.Reason = TEXT("Source file not found");
		ParseError.Hint = TEXT("Check AssetImportData source path or restore source file");

		const FString ErrorMsg = ParseError.ToContractString();
		UE_LOG(LogHCIAbilityKitFactory, Error, TEXT("Reimport failed: %s"), *ErrorMsg);
		ShowReimportFailureNotification(Asset, ErrorMsg);
		return EReimportResult::Failed;
	}

	FHCIAbilityKitParsedData ParsedData;
	FHCIAbilityKitParseError ParseError;
	// 重新调用解析逻辑处理最新的源文件内容
	if (!FHCIAbilityKitParserService::TryParseKitFile(FilenameToLoad, ParsedData, ParseError))
	{
		const FString ErrorMsg = ParseError.ToContractString();
		UE_LOG(LogHCIAbilityKitFactory, Error, TEXT("Reimport failed: %s"), *ErrorMsg);
		if (!ParseError.Detail.IsEmpty())
		{
			UE_LOG(LogHCIAbilityKitFactory, Error, TEXT("Reimport detail: %s"), *ParseError.Detail);
		}
		ShowReimportFailureNotification(Asset, ErrorMsg);
		return EReimportResult::Failed;
	}

	// 修改资产前记录快照，以支持虚幻引擎的撤销（Undo）系统
	Asset->Modify();
	ApplyParsedToAsset(Asset, ParsedData);
	Asset->AssetImportData->Update(FilenameToLoad);
	Asset->MarkPackageDirty();
	
	// 通知编辑器资产已完成更改
	Asset->PostEditChange();
	FHCIAbilityKitSearchIndexService::Get().RefreshAsset(Asset);
	return EReimportResult::Succeeded;
#else
	return EReimportResult::Failed;
#endif
}

void UHCIAbilityKitFactory::ApplyParsedToAsset(UHCIAbilityKitAsset* Asset, const FHCIAbilityKitParsedData& Parsed)
{
	if (!Asset)
	{
		return;
	}

	// 将解析出的结构化数据同步到资产属性中
	Asset->SchemaVersion = Parsed.SchemaVersion;
	Asset->Id = Parsed.Id;
	Asset->DisplayName = Parsed.DisplayName;
	Asset->Damage = Parsed.Damage;
}
