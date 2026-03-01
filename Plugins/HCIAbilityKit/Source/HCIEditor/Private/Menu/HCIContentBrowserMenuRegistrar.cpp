#include "Menu/HCIContentBrowserMenuRegistrar.h"

#include "ContentBrowserMenuContexts.h"
#include "Factories/HCIFactory.h"
#include "HCIAsset.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"

void FHCIContentBrowserMenuRegistrar::Startup(UHCIFactory* InFactory)
{
	Factory = InFactory;

	if (bStartupCallbackRegistered)
	{
		return;
	}

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FHCIContentBrowserMenuRegistrar::RegisterMenus));
	bStartupCallbackRegistered = true;
}

void FHCIContentBrowserMenuRegistrar::Shutdown()
{
	if (bStartupCallbackRegistered && UToolMenus::IsToolMenuUIEnabled())
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	}

	bStartupCallbackRegistered = false;
	Factory = nullptr;
}

void FHCIContentBrowserMenuRegistrar::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UHCIAsset::StaticClass());
	if (!Menu)
	{
		return;
	}

	FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("GetAssetActions"));
	Section.AddDynamicEntry(
		"HCI_Reimport",
		FNewToolMenuSectionDelegate::CreateRaw(this, &FHCIContentBrowserMenuRegistrar::AddReimportMenuEntry));
}

void FHCIContentBrowserMenuRegistrar::AddReimportMenuEntry(FToolMenuSection& InSection)
{
	const TAttribute<FText> Label = NSLOCTEXT("HCI", "Reimport", "Reimport");
	const TAttribute<FText> ToolTip = NSLOCTEXT("HCI", "Reimport_Tooltip", "Reimport from source .hciabilitykit file.");
	const FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.ReimportAsset");

	FToolUIAction UIAction;
	UIAction.ExecuteAction = FToolMenuExecuteAction::CreateRaw(this, &FHCIContentBrowserMenuRegistrar::ReimportSelectedAbilityKits);
	UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateRaw(this, &FHCIContentBrowserMenuRegistrar::CanReimportSelectedAbilityKits);
	InSection.AddMenuEntry("HCI_Reimport", Label, ToolTip, Icon, UIAction);
}

bool FHCIContentBrowserMenuRegistrar::CanReimportSelectedAbilityKits(const FToolMenuContext& MenuContext) const
{
	const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);
	if (!Context || !Factory)
	{
		return false;
	}

	for (UHCIAsset* Asset : Context->LoadSelectedObjects<UHCIAsset>())
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

void FHCIContentBrowserMenuRegistrar::ReimportSelectedAbilityKits(const FToolMenuContext& MenuContext) const
{
	const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);
	if (!Context || !Factory)
	{
		return;
	}

	for (UHCIAsset* Asset : Context->LoadSelectedObjects<UHCIAsset>())
	{
		Factory->Reimport(Asset);
	}
}

