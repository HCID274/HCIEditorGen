#pragma once

#include "CoreMinimal.h"

class UHCIFactory;
struct FToolMenuContext;
struct FToolMenuSection;

class FHCIContentBrowserMenuRegistrar
{
public:
	void Startup(UHCIFactory* InFactory);
	void Shutdown();

private:
	void RegisterMenus();
	void AddReimportMenuEntry(FToolMenuSection& InSection);
	bool CanReimportSelectedAbilityKits(const FToolMenuContext& MenuContext) const;
	void ReimportSelectedAbilityKits(const FToolMenuContext& MenuContext) const;

	UHCIFactory* Factory = nullptr;
	bool bStartupCallbackRegistered = false;
};

