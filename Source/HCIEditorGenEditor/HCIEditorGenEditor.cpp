 #include "HCIEditorGenEditor.h"

#include "ContentBrowserMenuContexts.h"
#include "Modules/ModuleManager.h"

#include "Factories/HCIAbilityKitFactory.h"
#include "HCIAbilityKitAsset.h"

#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "Misc/DelayedAutoRegister.h"

  static TObjectPtr<UHCIAbilityKitFactory> GHCIAbilityKitFactory;
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

  void FHCIEditorGenEditorModule::StartupModule()
  {
        // 让 Factory 常驻，保证随时可 Reimport（避免被 GC 掉）
        GHCIAbilityKitFactory = NewObject<UHCIAbilityKitFactory>(GetTransientPackage());
        GHCIAbilityKitFactory->AddToRoot();
  }

  void FHCIEditorGenEditorModule::ShutdownModule()
  {
        if (GHCIAbilityKitFactory)
        {
                GHCIAbilityKitFactory->RemoveFromRoot();
                GHCIAbilityKitFactory = nullptr;
        }
  }

  IMPLEMENT_MODULE(FHCIEditorGenEditorModule, HCIEditorGenEditor)
  static FDelayedAutoRegisterHelper
  HCIAbilityKit_MenuRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []
  {
        UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
        {
                FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);

                UToolMenu* Menu =   UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UHCIAbilityKitAsset::StaticClass());
                FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");

                Section.AddDynamicEntry("HCIAbilityKit_Reimport", FNewToolMenuSectionDelegate::CreateLambda([] 
  (FToolMenuSection& InSection)
                {
                        const TAttribute<FText> Label = NSLOCTEXT("HCIAbilityKit", "Reimport", "Reimport");
                        const TAttribute<FText> ToolTip = NSLOCTEXT("HCIAbilityKit", "Reimport_Tooltip", "Reimport from source .hciabilitykit file.");
                        const FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.ReimportAsset");

                        FToolUIAction UIAction;
                        UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&HCI_ReimportSelectedAbilityKits);
                        UIAction.CanExecuteAction =   FToolMenuCanExecuteAction::CreateStatic(&HCI_CanReimportSelectedAbilityKits);

                        InSection.AddMenuEntry("HCIAbilityKit_Reimport", Label, ToolTip, Icon, UIAction);
                }));
        }));
  });