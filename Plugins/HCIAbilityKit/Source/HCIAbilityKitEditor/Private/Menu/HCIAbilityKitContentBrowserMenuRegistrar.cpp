#include "Menu/HCIAbilityKitContentBrowserMenuRegistrar.h"

#include "ContentBrowserMenuContexts.h"
#include "Factories/HCIAbilityKitFactory.h"
#include "HCIAbilityKitAsset.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"

void FHCIAbilityKitContentBrowserMenuRegistrar::Startup(UHCIAbilityKitFactory* InFactory)
{
	Factory = InFactory;

	if (bStartupCallbackRegistered)
	{
		return;
	}

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FHCIAbilityKitContentBrowserMenuRegistrar::RegisterMenus));
	bStartupCallbackRegistered = true;
}

void FHCIAbilityKitContentBrowserMenuRegistrar::Shutdown()
{
	if (bStartupCallbackRegistered && UToolMenus::IsToolMenuUIEnabled())
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	}

	bStartupCallbackRegistered = false;
	Factory = nullptr;
}

void FHCIAbilityKitContentBrowserMenuRegistrar::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UHCIAbilityKitAsset::StaticClass());
	if (!Menu)
	{
		return;
	}

	FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("GetAssetActions"));
	Section.AddDynamicEntry(
		"HCIAbilityKit_Reimport",
		FNewToolMenuSectionDelegate::CreateRaw(this, &FHCIAbilityKitContentBrowserMenuRegistrar::AddReimportMenuEntry));
}

void FHCIAbilityKitContentBrowserMenuRegistrar::AddReimportMenuEntry(FToolMenuSection& InSection)
{
	const TAttribute<FText> Label = NSLOCTEXT("HCIAbilityKit", "Reimport", "Reimport");
	const TAttribute<FText> ToolTip = NSLOCTEXT("HCIAbilityKit", "Reimport_Tooltip", "Reimport from source .hciabilitykit file.");
	const FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.ReimportAsset");

	FToolUIAction UIAction;
	UIAction.ExecuteAction = FToolMenuExecuteAction::CreateRaw(this, &FHCIAbilityKitContentBrowserMenuRegistrar::ReimportSelectedAbilityKits);
	UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateRaw(this, &FHCIAbilityKitContentBrowserMenuRegistrar::CanReimportSelectedAbilityKits);
	InSection.AddMenuEntry("HCIAbilityKit_Reimport", Label, ToolTip, Icon, UIAction);
}

bool FHCIAbilityKitContentBrowserMenuRegistrar::CanReimportSelectedAbilityKits(const FToolMenuContext& MenuContext) const
{
	const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);
	if (!Context || !Factory)
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
		if (Factory->CanReimport(Asset, SourceFiles))
		{
			return true;
		}
	}

	return false;
}

void FHCIAbilityKitContentBrowserMenuRegistrar::ReimportSelectedAbilityKits(const FToolMenuContext& MenuContext) const
{
	const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);
	if (!Context || !Factory)
	{
		return;
	}

	for (UHCIAbilityKitAsset* Asset : Context->LoadSelectedObjects<UHCIAbilityKitAsset>())
	{
		Factory->Reimport(Asset);
	}
}
