#include "AgentActions/HCIAgentToolActions.h"

#include "AgentActions/ToolActions/HCIToolActionFactories.h"

void HCIAgentToolActions::BuildStageIDraftActions(TMap<FName, TSharedPtr<IHCIAgentToolAction>>& OutActions)
{
	OutActions.Reset();
	OutActions.Add(TEXT("ScanAssets"), HCIToolActionFactories::MakeScanAssetsToolAction());
	OutActions.Add(TEXT("ScanMeshTriangleCount"), HCIToolActionFactories::MakeScanMeshTriangleCountToolAction());
	OutActions.Add(TEXT("SearchPath"), HCIToolActionFactories::MakeSearchPathToolAction());
	OutActions.Add(TEXT("ScanLevelMeshRisks"), HCIToolActionFactories::MakeScanLevelMeshRisksToolAction());
	OutActions.Add(TEXT("SetTextureMaxSize"), HCIToolActionFactories::MakeSetTextureMaxSizeToolAction());
	OutActions.Add(TEXT("SetMeshLODGroup"), HCIToolActionFactories::MakeSetMeshLODGroupToolAction());
	OutActions.Add(TEXT("NormalizeAssetNamingByMetadata"), HCIToolActionFactories::MakeNormalizeAssetNamingByMetadataToolAction());
	OutActions.Add(TEXT("AutoMaterialSetupByNameContract"), HCIToolActionFactories::MakeAutoMaterialSetupByNameContractToolAction());
	OutActions.Add(TEXT("RenameAsset"), HCIToolActionFactories::MakeRenameAssetToolAction());
	OutActions.Add(TEXT("MoveAsset"), HCIToolActionFactories::MakeMoveAssetToolAction());
}


