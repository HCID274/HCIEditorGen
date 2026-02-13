#include "Factories/HCIAbilityKitFactory.h"

#include "HCIAbilityKitAsset.h"
#include "Services/HCIAbilityKitParserService.h"

#include "EditorFramework/AssetImportData.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/FeedbackContext.h"
#include "Misc/Paths.h"
#include "Widgets/Notifications/SNotificationList.h"

// 定义工厂专用的日志分类喵
DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitFactory, Log, All);

namespace
{
/**
 * 在编辑器右下角弹出重导入失败的通知提醒喵
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
	// 注册支持的文件扩展名及其描述喵
	Formats.Add(TEXT("hciabilitykit;HCI Ability Kit (*.hciabilitykit)"));
	// 设置此工厂产出的资产类型为 UHCIAbilityKitAsset 喵
	SupportedClass = UHCIAbilityKitAsset::StaticClass();
	// 设置为不支持手动新建，必须通过导入源文件来创建喵
	bCreateNew = false;
	bText = true;
	bEditorImport = true;
}

bool UHCIAbilityKitFactory::FactoryCanImport(const FString& Filename)
{
	// 检查文件后缀是否是我们支持的 .hciabilitykit 喵
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
	
	// 调用解析服务来尝试解析源文件喵
	if (!FHCIAbilityKitParserService::TryParseKitFile(Filename, ParsedData, ParseError))
	{
		// 解析失败，获取符合契约格式的错误字符串喵
		const FString ErrorMsg = ParseError.ToContractString();
		if (Warn)
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("HCIAbilityKit import failed: %s"), *ErrorMsg);
		}
		UE_LOG(LogHCIAbilityKitFactory, Error, TEXT("Import failed: %s"), *ErrorMsg);
		
		// 如果有详细的错误描述，也一并记录下来喵
		if (!ParseError.Detail.IsEmpty())
		{
			UE_LOG(LogHCIAbilityKitFactory, Error, TEXT("Import detail: %s"), *ParseError.Detail);
		}
		
		bOutOperationCanceled = true;
		return nullptr;
	}

	// 成功解析后，创建资产对象喵
	UHCIAbilityKitAsset* NewAsset = NewObject<UHCIAbilityKitAsset>(InParent, InClass, InName, Flags);
	if (!NewAsset)
	{
		bOutOperationCanceled = true;
		return nullptr;
	}

	// 将解析出来的数据填入资产喵
	ApplyParsedToAsset(NewAsset, ParsedData);

#if WITH_EDITORONLY_DATA
	// 更新资产的导入源数据，用于日后的“重导入”操作喵
	if (NewAsset->AssetImportData)
	{
		const FString AbsoluteFilename = FPaths::ConvertRelativePathToFull(Filename);
		NewAsset->AssetImportData->Update(AbsoluteFilename);
	}
#endif

	// 标记资产为已修改状态喵
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
	// 从资产中提取原始导入时的源文件路径喵
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
	// 更新资产记录的重导入路径喵
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

	// 统一转换为绝对路径喵
	FString FilenameToLoad = Files[0];
	if (FPaths::IsRelative(FilenameToLoad))
	{
		FilenameToLoad = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), FilenameToLoad);
	}

	// 如果物理文件不见了，构造一个错误对象喵
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
	// 重新执行解析逻辑喵
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

	// 资产数据更新前标记 Modify，以便支持撤销喵
	Asset->Modify();
	ApplyParsedToAsset(Asset, ParsedData);
	Asset->AssetImportData->Update(FilenameToLoad);
	Asset->MarkPackageDirty();
	
	// 通知引擎资产已变更喵
	Asset->PostEditChange();
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

	// 同步数据到资产属性喵
	Asset->SchemaVersion = Parsed.SchemaVersion;
	Asset->Id = Parsed.Id;
	Asset->DisplayName = Parsed.DisplayName;
	Asset->Damage = Parsed.Damage;
}
