#include "HCIAbilityKitEditorModule.h"

#include "ContentBrowserMenuContexts.h"
#include "Dom/JsonObject.h"
#include "Factories/HCIAbilityKitFactory.h"
#include "HAL/FileManager.h"
#include "HCIAbilityKitAsset.h"
#include "IPythonScriptPlugin.h"
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

/** 
 * 全局持久化变量喵 
 */
static TObjectPtr<UHCIAbilityKitFactory> GHCIAbilityKitFactory; // 缓存一个工厂实例，用于菜单操作喵
static const TCHAR* GHCIAbilityKitPythonScriptPath = TEXT("SourceData/AbilityKits/Python/hci_abilitykit_hook.py"); // Python 脚本的相对路径喵

static FString HCI_ToPythonStringLiteral(const FString& Value)
{
	FString Escaped = Value;
	Escaped.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
	Escaped.ReplaceInline(TEXT("'"), TEXT("\\'"));
	return FString::Printf(TEXT("'%s'"), *Escaped);
}

static FString HCI_GetJsonStringOrDefault(
	const TSharedPtr<FJsonObject>& JsonObject,
	const FString& FieldName,
	const FString& DefaultValue)
{
	if (!JsonObject.IsValid())
	{
		return DefaultValue;
	}

	FString Parsed;
	return JsonObject->TryGetStringField(FieldName, Parsed) ? Parsed : DefaultValue;
}

static void HCI_DeleteTempFile(const FString& Filename)
{
	if (!Filename.IsEmpty())
	{
		IFileManager::Get().Delete(*Filename, false, true, true);
	}
}

static bool HCI_ParsePythonResponse(
	const FString& ResponseFilename,
	const FString& ScriptPath,
	const FString& SourceFilename,
	FHCIAbilityKitParsedData& InOutParsed,
	FHCIAbilityKitParseError& OutError)
{
	FString ResponseContent;
	if (!FFileHelper::LoadFileToString(ResponseContent, *ResponseFilename))
	{
		OutError.Code = TEXT("E3001");
		OutError.File = ScriptPath;
		OutError.Field = TEXT("python_response");
		OutError.Reason = TEXT("Python hook response file is missing");
		OutError.Hint = TEXT("Check Python script output and response path");
		OutError.Detail = ResponseFilename;
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError.Code = TEXT("E3001");
		OutError.File = ScriptPath;
		OutError.Field = TEXT("python_response");
		OutError.Reason = TEXT("Python hook response is not valid JSON");
		OutError.Hint = TEXT("Return valid JSON with {ok, patch, error, audit}");
		OutError.Detail = ResponseContent;
		return false;
	}

	bool bPythonOk = false;
	if (!RootObject->TryGetBoolField(TEXT("ok"), bPythonOk))
	{
		OutError.Code = TEXT("E3001");
		OutError.File = ScriptPath;
		OutError.Field = TEXT("python_response.ok");
		OutError.Reason = TEXT("Python hook response missing required bool field: ok");
		OutError.Hint = TEXT("Set response.ok to true or false");
		return false;
	}

	if (!bPythonOk)
	{
		const TSharedPtr<FJsonObject>* ErrorObject = nullptr;
		if (RootObject->TryGetObjectField(TEXT("error"), ErrorObject) && ErrorObject && ErrorObject->IsValid())
		{
			OutError.Code = HCI_GetJsonStringOrDefault(*ErrorObject, TEXT("code"), TEXT("E3001"));
			OutError.File = SourceFilename;
			OutError.Field = HCI_GetJsonStringOrDefault(*ErrorObject, TEXT("field"), TEXT("python_hook"));
			OutError.Reason = HCI_GetJsonStringOrDefault(*ErrorObject, TEXT("reason"), TEXT("Python hook returned failure"));
			OutError.Hint = HCI_GetJsonStringOrDefault(*ErrorObject, TEXT("hint"), TEXT("Inspect Python response error payload"));
			OutError.Detail = HCI_GetJsonStringOrDefault(*ErrorObject, TEXT("detail"), FString());
		}
		else
		{
			OutError.Code = TEXT("E3001");
			OutError.File = SourceFilename;
			OutError.Field = TEXT("python_hook");
			OutError.Reason = TEXT("Python hook returned failure without error payload");
			OutError.Hint = TEXT("Return response.error object when ok=false");
			OutError.Detail = ResponseContent;
		}
		return false;
	}

	const TSharedPtr<FJsonObject>* PatchObject = nullptr;
	if (RootObject->TryGetObjectField(TEXT("patch"), PatchObject) && PatchObject && PatchObject->IsValid())
	{
		FString DisplayNamePatch;
		if ((*PatchObject)->TryGetStringField(TEXT("display_name"), DisplayNamePatch))
		{
			InOutParsed.DisplayName = DisplayNamePatch;
		}
	}

	return true;
}

/**
 * 封装 Python 脚本的调用逻辑喵
 * 负责通过 IPythonScriptPlugin 接口在 C++ 中执行 Python 脚本喵
 */
static bool HCI_RunPythonHook(const FString& SourceFilename, FHCIAbilityKitParsedData& InOutParsed, FHCIAbilityKitParseError& OutError)
{
	// 将相对路径转换为绝对路径，确保 Python 能找到它喵
	const FString ScriptPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), GHCIAbilityKitPythonScriptPath);
	if (!FPaths::FileExists(ScriptPath))
	{
		// 如果脚本不存在，手动填充错误契约信息喵
		OutError.Code = TEXT("E3002");
		OutError.File = ScriptPath;
		OutError.Field = TEXT("python_hook");
		OutError.Reason = TEXT("Python hook script not found");
		OutError.Hint = TEXT("Restore script file in project directory");
		return false;
	}

	// 获取 Python 脚本插件的接口喵
	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin || !PythonPlugin->IsPythonAvailable())
	{
		OutError.Code = TEXT("E3003");
		OutError.File = ScriptPath;
		OutError.Field = TEXT("python_plugin");
		OutError.Reason = TEXT("PythonScriptPlugin is unavailable");
		OutError.Hint = TEXT("Enable PythonScriptPlugin in project settings");
		return false;
	}

	const FString ResponseDir = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("HCIAbilityKit"), TEXT("Python"));
	IFileManager::Get().MakeDirectory(*ResponseDir, true);

	const FString ResponseFilename = FPaths::Combine(
		ResponseDir,
		FString::Printf(TEXT("hook_response_%s.json"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));

	// 构造 Python 命令喵
	// 注意：UE ExecuteFile 不保证传递 argv，这里改为 ExecuteStatement + runpy 显式注入 sys.argv 喵
	const FString PythonStatement = FString::Printf(
		TEXT("import runpy, sys; sys.argv=[%s, %s, %s]; runpy.run_path(%s, run_name='__main__')"),
		*HCI_ToPythonStringLiteral(ScriptPath),
		*HCI_ToPythonStringLiteral(SourceFilename),
		*HCI_ToPythonStringLiteral(ResponseFilename),
		*HCI_ToPythonStringLiteral(ScriptPath));

	FPythonCommandEx Command;
	Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
	Command.Command = PythonStatement;

	// 执行命令喵
	const bool bExecOk = PythonPlugin->ExecPythonCommandEx(Command);
	if (!bExecOk)
	{
		// 如果执行失败，尝试解析 Python 抛出的错误信息喵
		OutError.Code = TEXT("E3001");
		OutError.File = ScriptPath;
		OutError.Field = TEXT("python_execution");
		OutError.Reason = Command.CommandResult.IsEmpty() ? TEXT("Python hook execution failed") : Command.CommandResult;
		OutError.Hint = TEXT("Check Python script for syntax errors or runtime exceptions");
		HCI_DeleteTempFile(ResponseFilename);
		return false;
	}

	const bool bParseOk = HCI_ParsePythonResponse(ResponseFilename, ScriptPath, SourceFilename, InOutParsed, OutError);
	HCI_DeleteTempFile(ResponseFilename);
	return bParseOk;
}

/** 
 * 菜单项可见性检查：判断当前选中的资产是否支持重导入喵 
 */
static bool HCI_CanReimportSelectedAbilityKits(const FToolMenuContext& MenuContext)
{
	const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);
	if (!Context || !GHCIAbilityKitFactory)
	{
		return false;
	}

	// 遍历所有选中的对象喵
	for (UHCIAbilityKitAsset* Asset : Context->LoadSelectedObjects<UHCIAbilityKitAsset>())
	{
		if (!Asset)
		{
			continue;
		}

		TArray<FString> SourceFiles;
		// 只要有一个资产支持重导入，菜单就可见喵
		if (GHCIAbilityKitFactory->CanReimport(Asset, SourceFiles))
		{
			return true;
		}
	}

	return false;
}

/** 
 * 菜单项点击回调：执行选中资产的重导入逻辑喵 
 */
static void HCI_ReimportSelectedAbilityKits(const FToolMenuContext& MenuContext)
{
	const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);
	if (!Context || !GHCIAbilityKitFactory)
	{
		return;
	}

	for (UHCIAbilityKitAsset* Asset : Context->LoadSelectedObjects<UHCIAbilityKitAsset>())
	{
		// 依次触发重导入流程喵
		GHCIAbilityKitFactory->Reimport(Asset);
	}
}

void FHCIAbilityKitEditorModule::StartupModule()
{
	// 1. 在运行时解析服务中注入 Python 钩子逻辑喵
	// 使用 Lambda 表达式进行转发喵
	FHCIAbilityKitParserService::SetPythonHook(
		[](const FString& SourceFilename, FHCIAbilityKitParsedData& InOutParsed, FHCIAbilityKitParseError& OutError)
		{
			return HCI_RunPythonHook(SourceFilename, InOutParsed, OutError);
		});

	// 2. 创建并锁定一个工厂实例喵
	// 这样做是为了在不需要弹出导入窗口的情况下，复用工厂的重导入逻辑喵
	GHCIAbilityKitFactory = NewObject<UHCIAbilityKitFactory>(GetTransientPackage());
	GHCIAbilityKitFactory->AddToRoot(); // 防止被 GC 喵
}

void FHCIAbilityKitEditorModule::ShutdownModule()
{
	// 1. 清理 Python 钩子，防止野指针调用喵
	FHCIAbilityKitParserService::ClearPythonHook();

	// 2. 释放工厂实例喵
	if (GHCIAbilityKitFactory)
	{
		GHCIAbilityKitFactory->RemoveFromRoot();
		GHCIAbilityKitFactory = nullptr;
	}
}

IMPLEMENT_MODULE(FHCIAbilityKitEditorModule, HCIAbilityKitEditor)

/** 
 * 静态注册：使用延时注册机制来在引擎启动最后阶段扩展菜单喵 
 */
static FDelayedAutoRegisterHelper HCIAbilityKitMenuRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);

		// 针对 UHCIAbilityKitAsset 类资产，扩展其内容浏览器的上下文（右键）菜单喵
		UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UHCIAbilityKitAsset::StaticClass());
		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");

		// 添加一个名为 Reimport 的菜单项喵
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
