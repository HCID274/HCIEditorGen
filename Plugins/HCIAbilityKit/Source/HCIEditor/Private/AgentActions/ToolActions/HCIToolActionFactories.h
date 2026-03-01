#pragma once

#include "CoreMinimal.h"

#include "Agent/Tools/HCIAgentToolAction.h"

namespace HCIToolActionFactories
{
TSharedPtr<IHCIAgentToolAction> MakeScanAssetsToolAction();
TSharedPtr<IHCIAgentToolAction> MakeScanMeshTriangleCountToolAction();
TSharedPtr<IHCIAgentToolAction> MakeSearchPathToolAction();
TSharedPtr<IHCIAgentToolAction> MakeScanLevelMeshRisksToolAction();
TSharedPtr<IHCIAgentToolAction> MakeSetTextureMaxSizeToolAction();
TSharedPtr<IHCIAgentToolAction> MakeSetMeshLODGroupToolAction();
TSharedPtr<IHCIAgentToolAction> MakeNormalizeAssetNamingByMetadataToolAction();
TSharedPtr<IHCIAgentToolAction> MakeAutoMaterialSetupByNameContractToolAction();
TSharedPtr<IHCIAgentToolAction> MakeRenameAssetToolAction();
TSharedPtr<IHCIAgentToolAction> MakeMoveAssetToolAction();
}

