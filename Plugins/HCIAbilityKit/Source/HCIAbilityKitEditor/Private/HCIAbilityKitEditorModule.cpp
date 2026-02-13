#include "HCIAbilityKitEditorModule.h"

#include "ContentBrowserMenuContexts.h"
#include "Factories/HCIAbilityKitFactory.h"
#include "HCIAbilityKitAsset.h"
#include "IPythonScriptPlugin.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Services/HCIAbilityKitParserService.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"

/** 用于重导入操作的全局工厂实例喵 */
static TObjectPtr<UHCIAbilityKitFactory> GHCIAbilityKitFactory;
static const TCHAR* GHCIAbilityKitPythonScriptPath = TEXT("SourceData/AbilityKits/Python/hci_abilitykit_hook.py");
DEFINE_LOG_CATEGORY_STATIC(LogHCIAbilityKitPythonHook, Log, All);

namespace
{
FHCIAbilityKitParseError MakePythonError(
	const FString& SourceFilename,
	const FString& Field,
	const FString& Reason,
	const FString& Hint,
	const FString& Detail = FString(),
	const FString& Code = TEXT("E3001"))
{
	FHCIAbilityKitParseError ParseError;
	ParseError.Code = Code;
	ParseError.File = SourceFilename;
	ParseError.Field = Field;
	ParseError.Reason = Reason;
	ParseError.Hint = Hint;
	ParseError.Detail = Detail;
	return ParseError;
}

bool ReadPythonHookResponse(
	const FString& SourceFilename,
	const FString& ResponseFile,
	FHCIAbilityKitParsedData& InOutParsed,
	FHCIAbilityKitParseError& OutError)
{
	FString ResponseText;
	if (!FFileHelper::LoadFileToString(ResponseText, *ResponseFile))
	{
		OutError = MakePythonError(
			SourceFilename,
			TEXT("python_hook_response"),
			TEXT("Missing Python hook response file"),
			TEXT("Check whether Python script writes the response JSON"));
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = MakePythonError(
			SourceFilename,
			TEXT("python_hook_response"),
			TEXT("Invalid Python response JSON format"),
			TEXT("Ensure Python returns valid JSON object"));
		return false;
	}

	bool bOk = false;
	if (!RootObject->TryGetBoolField(TEXT("ok"), bOk))
	{
		OutError = MakePythonError(
			SourceFilename,
			TEXT("python_hook_response.ok"),
			TEXT("Missing required field: ok"),
			TEXT("Python response must include boolean field ok"));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* AuditArray = nullptr;
	if (RootObject->TryGetArrayField(TEXT("audit"), AuditArray) && AuditArray)
	{
		for (const TSharedPtr<FJsonValue>& AuditValue : *AuditArray)
		{
			const TSharedPtr<FJsonObject> AuditObj = AuditValue.IsValid() ? AuditValue->AsObject() : nullptr;
			if (!AuditObj.IsValid())
			{
				continue;
			}

			FString Level;
			FString Code;
			FString Message;
			AuditObj->TryGetStringField(TEXT("level"), Level);
			AuditObj->TryGetStringField(TEXT("code"), Code);
			AuditObj->TryGetStringField(TEXT("message"), Message);
			UE_LOG(LogHCIAbilityKitPythonHook, Display, TEXT("[HCIAbilityKit][PythonAudit] level=%s code=%s message=%s"), *Level, *Code, *Message);
		}
	}

	if (!bOk)
	{
		const TSharedPtr<FJsonObject>* ErrorObj = nullptr;
		if (RootObject->TryGetObjectField(TEXT("error"), ErrorObj) && ErrorObj && ErrorObj->IsValid())
		{
			FString ErrorCode;
			FString Field;
			FString Reason;
			FString Hint;
			FString Detail;
			(*ErrorObj)->TryGetStringField(TEXT("code"), ErrorCode);
			(*ErrorObj)->TryGetStringField(TEXT("field"), Field);
			(*ErrorObj)->TryGetStringField(TEXT("reason"), Reason);
			(*ErrorObj)->TryGetStringField(TEXT("hint"), Hint);
			(*ErrorObj)->TryGetStringField(TEXT("detail"), Detail);

			OutError = MakePythonError(
				SourceFilename,
				Field.IsEmpty() ? TEXT("python_hook") : Field,
				Reason.IsEmpty() ? TEXT("Python hook reported failure") : Reason,
				Hint.IsEmpty() ? TEXT("Check Python audit output") : Hint,
				Detail,
				ErrorCode.IsEmpty() ? TEXT("E3001") : ErrorCode);
			return false;
		}

		OutError = MakePythonError(
			SourceFilename,
			TEXT("python_hook"),
			TEXT("Python hook reported failure without error payload"),
			TEXT("Return an error object with code/field/reason/hint"));
		return false;
	}

	const TSharedPtr<FJsonObject>* PatchObj = nullptr;
	if (RootObject->TryGetObjectField(TEXT("patch"), PatchObj) && PatchObj && PatchObj->IsValid())
	{
		if ((*PatchObj)->HasField(TEXT("display_name")))
		{
			FString DisplayName;
			if (!(*PatchObj)->TryGetStringField(TEXT("display_name"), DisplayName))
			{
				OutError = MakePythonError(
					SourceFilename,
					TEXT("patch.display_name"),
					TEXT("Invalid patch field type"),
					TEXT("patch.display_name must be a string"));
				return false;
			}

			InOutParsed.DisplayName = DisplayName;
		}
	}

	return true;
}
}

static bool HCI_RunPythonHook(const FString& SourceFilename, FHCIAbilityKitParsedData& InOutParsed, FHCIAbilityKitParseError& OutError)
{
	const FString ScriptPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), GHCIAbilityKitPythonScriptPath);
	if (!FPaths::FileExists(ScriptPath))
	{
		OutError = MakePythonError(
			SourceFilename,
			TEXT("python_hook"),
			TEXT("Python hook script not found"),
			TEXT("Create script under SourceData/AbilityKits/Python"),
			ScriptPath);
		return false;
	}

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin || !PythonPlugin->IsPythonAvailable())
	{
		OutError = MakePythonError(
			SourceFilename,
			TEXT("python_hook"),
			TEXT("PythonScriptPlugin is unavailable"),
			TEXT("Enable PythonScriptPlugin in project/plugin"));
		return false;
	}

	const FString OutputDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HCIAbilityKit"), TEXT("Python"));
	IFileManager::Get().MakeDirectory(*OutputDir, true);
	const FString OutputFile = FPaths::Combine(OutputDir, FString::Printf(TEXT("hook_result_%s.json"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));

	FPythonCommandEx Command;
	Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
	Command.FileExecutionScope = EPythonFileExecutionScope::Private;
	Command.Command = FString::Printf(TEXT("\"%s\" \"%s\" \"%s\""), *ScriptPath, *SourceFilename, *OutputFile);

	const bool bOk = PythonPlugin->ExecPythonCommandEx(Command);
	if (!bOk)
	{
		OutError = MakePythonError(
			SourceFilename,
			TEXT("python_hook"),
			TEXT("Python hook execution failed"),
			TEXT("Inspect traceback in detail and fix script logic"),
			Command.CommandResult.IsEmpty() ? ScriptPath : Command.CommandResult);
		IFileManager::Get().Delete(*OutputFile, false, true, true);
		return false;
	}

	const bool bResponseOk = ReadPythonHookResponse(SourceFilename, OutputFile, InOutParsed, OutError);
	IFileManager::Get().Delete(*OutputFile, false, true, true);
	if (!bResponseOk)
	{
		return false;
	}

	return true;
}

/** 检查选中的资产是否可以被重导入喵 */
static bool HCI_CanReimportSelectedAbilityKits(const FToolMenuContext& MenuContext)
{
	const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);
	if (!Context || !GHCIAbilityKitFactory)
	{
		return false;
	}

	for (UHCIAbilityKitAsset* Asset : Context->LoadSelectedObjects<UHCIAbilityKitAsset>())
	{
		if (!Asset)
		{
			continue;
		}

		TArray<FString> SourceFiles;
		if (GHCIAbilityKitFactory->CanReimport(Asset, SourceFiles))
		{
			return true;
		}
	}

	return false;
}

/** 执行选中资产的重导入逻辑喵 */
static void HCI_ReimportSelectedAbilityKits(const FToolMenuContext& MenuContext)
{
	const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);
	if (!Context || !GHCIAbilityKitFactory)
	{
		return;
	}

	for (UHCIAbilityKitAsset* Asset : Context->LoadSelectedObjects<UHCIAbilityKitAsset>())
	{
		GHCIAbilityKitFactory->Reimport(Asset);
	}
}

void FHCIAbilityKitEditorModule::StartupModule()
{
	FHCIAbilityKitParserService::SetPythonHook(
		[](const FString& SourceFilename, FHCIAbilityKitParsedData& InOutParsed, FHCIAbilityKitParseError& OutError)
		{
			return HCI_RunPythonHook(SourceFilename, InOutParsed, OutError);
		});

	// 创建一个持久化的工厂对象用于处理菜单操作喵
	GHCIAbilityKitFactory = NewObject<UHCIAbilityKitFactory>(GetTransientPackage());
	GHCIAbilityKitFactory->AddToRoot();
}

void FHCIAbilityKitEditorModule::ShutdownModule()
{
	FHCIAbilityKitParserService::ClearPythonHook();

	// 清理工厂对象喵
	if (GHCIAbilityKitFactory)
	{
		GHCIAbilityKitFactory->RemoveFromRoot();
		GHCIAbilityKitFactory = nullptr;
	}
}

IMPLEMENT_MODULE(FHCIAbilityKitEditorModule, HCIAbilityKitEditor)

/** 使用延时注册机制来在引擎初始化完成后注册内容浏览器菜单扩展喵 */
static FDelayedAutoRegisterHelper HCIAbilityKitMenuRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);

		// 扩展 UHCIAbilityKitAsset 类型资产的内容浏览器右键菜单喵
		UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UHCIAbilityKitAsset::StaticClass());
		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");

		// 添加动态菜单项“Reimport”喵
		Section.AddDynamicEntry("HCIAbilityKit_Reimport", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			const TAttribute<FText> Label = NSLOCTEXT("HCIAbilityKit", "Reimport", "Reimport");
			const TAttribute<FText> ToolTip = NSLOCTEXT("HCIAbilityKit", "Reimport_Tooltip", "Reimport from source .hciabilitykit file.");
			const FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.ReimportAsset");

			FToolUIAction UIAction;
			UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&HCI_ReimportSelectedAbilityKits);
			UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&HCI_CanReimportSelectedAbilityKits);

			InSection.AddMenuEntry("HCIAbilityKit_Reimport", Label, ToolTip, Icon, UIAction);
		}));
	}));
});
