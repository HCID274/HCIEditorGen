#pragma once

#include "CoreMinimal.h"

class UHCIAbilityKitFactory;
struct FToolMenuContext;
struct FToolMenuSection;

class FHCIAbilityKitContentBrowserMenuRegistrar
{
public:
	void Startup(UHCIAbilityKitFactory* InFactory);
	void Shutdown();

private:
	void RegisterMenus();
	void AddReimportMenuEntry(FToolMenuSection& InSection);
	bool CanReimportSelectedAbilityKits(const FToolMenuContext& MenuContext) const;
	void ReimportSelectedAbilityKits(const FToolMenuContext& MenuContext) const;

	UHCIAbilityKitFactory* Factory = nullptr;
	bool bStartupCallbackRegistered = false;
};
