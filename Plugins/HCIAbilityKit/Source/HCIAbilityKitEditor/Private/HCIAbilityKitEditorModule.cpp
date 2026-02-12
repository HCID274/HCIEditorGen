#include "HCIAbilityKitEditorModule.h"

#include "ContentBrowserMenuContexts.h"
#include "Factories/HCIAbilityKitFactory.h"
#include "HCIAbilityKitAsset.h"
#include "IPythonScriptPlugin.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Services/HCIAbilityKitParserService.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"

/** 用于重导入操作的全局工厂实例喵 */
static TObjectPtr<UHCIAbilityKitFactory> GHCIAbilityKitFactory;
static const TCHAR* GHCIAbilityKitPythonScriptPath = TEXT("SourceData/AbilityKits/Python/hci_abilitykit_hook.py");

static bool HCI_RunPythonHook(const FString& SourceFilename, FHCIAbilityKitParsedData& InOutParsed, FString& OutError)
{
	(void)InOutParsed;

	const FString ScriptPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), GHCIAbilityKitPythonScriptPath);
	if (!FPaths::FileExists(ScriptPath))
	{
		OutError = FString::Printf(TEXT("Python hook script not found: %s"), *ScriptPath);
		return false;
	}

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin || !PythonPlugin->IsPythonAvailable())
	{
		OutError = TEXT("PythonScriptPlugin is unavailable");
		return false;
	}

	FPythonCommandEx Command;
	Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
	Command.FileExecutionScope = EPythonFileExecutionScope::Private;
	Command.Command = FString::Printf(TEXT("\"%s\" \"%s\""), *ScriptPath, *SourceFilename);

	const bool bOk = PythonPlugin->ExecPythonCommandEx(Command);
	if (!bOk)
	{
		if (!Command.CommandResult.IsEmpty())
		{
			OutError = Command.CommandResult;
		}
		else
		{
			OutError = FString::Printf(TEXT("Python hook execution failed: %s"), *ScriptPath);
		}
	}

	return bOk;
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
		[](const FString& SourceFilename, FHCIAbilityKitParsedData& InOutParsed, FString& OutError)
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
