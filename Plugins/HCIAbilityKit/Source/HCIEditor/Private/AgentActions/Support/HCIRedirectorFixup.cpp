#include "AgentActions/Support/HCIRedirectorFixup.h"

#include "AssetToolsModule.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectRedirector.h"

namespace HCIRedirectorFixup
{
FHCIRedirectorFixupResult FixupRedirectorReferencers(const FString& SourceAssetPath)
{
	FHCIRedirectorFixupResult Result;

	UObjectRedirector* Redirector = Cast<UObjectRedirector>(LoadObject<UObject>(nullptr, *SourceAssetPath));
	if (Redirector == nullptr)
	{
		Result.Status = TEXT("no_redirector_found");
		Result.RedirectorCount = 0;
		return Result;
	}

	TArray<UObjectRedirector*> Redirectors;
	Redirectors.Add(Redirector);

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	AssetToolsModule.Get().FixupReferencers(Redirectors, false, ERedirectFixupMode::DeleteFixedUpRedirectors);

	Result.Status = TEXT("fixup_referencers_called");
	Result.RedirectorCount = Redirectors.Num();
	return Result;
}
}


