#include "AgentActions/HCIAbilityKitAgentToolActions.h"

#include "AgentActions/ToolActions/HCIAbilityKitToolActionFactories.h"

void HCIAbilityKitAgentToolActions::BuildStageIDraftActions(TMap<FName, TSharedPtr<IHCIAbilityKitAgentToolAction>>& OutActions)
{
	OutActions.Reset();
	OutActions.Add(TEXT("ScanAssets"), HCIAbilityKitToolActionFactories::MakeScanAssetsToolAction());
	OutActions.Add(TEXT("ScanMeshTriangleCount"), HCIAbilityKitToolActionFactories::MakeScanMeshTriangleCountToolAction());
	OutActions.Add(TEXT("SearchPath"), HCIAbilityKitToolActionFactories::MakeSearchPathToolAction());
	OutActions.Add(TEXT("ScanLevelMeshRisks"), HCIAbilityKitToolActionFactories::MakeScanLevelMeshRisksToolAction());
	OutActions.Add(TEXT("SetTextureMaxSize"), HCIAbilityKitToolActionFactories::MakeSetTextureMaxSizeToolAction());
	OutActions.Add(TEXT("SetMeshLODGroup"), HCIAbilityKitToolActionFactories::MakeSetMeshLODGroupToolAction());
	OutActions.Add(TEXT("NormalizeAssetNamingByMetadata"), HCIAbilityKitToolActionFactories::MakeNormalizeAssetNamingByMetadataToolAction());
	OutActions.Add(TEXT("AutoMaterialSetupByNameContract"), HCIAbilityKitToolActionFactories::MakeAutoMaterialSetupByNameContractToolAction());
	OutActions.Add(TEXT("RenameAsset"), HCIAbilityKitToolActionFactories::MakeRenameAssetToolAction());
	OutActions.Add(TEXT("MoveAsset"), HCIAbilityKitToolActionFactories::MakeMoveAssetToolAction());
}

