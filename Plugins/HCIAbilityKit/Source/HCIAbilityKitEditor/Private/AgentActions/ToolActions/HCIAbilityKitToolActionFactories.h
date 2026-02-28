#pragma once

#include "CoreMinimal.h"

#include "Agent/Tools/HCIAbilityKitAgentToolAction.h"

namespace HCIAbilityKitToolActionFactories
{
TSharedPtr<IHCIAbilityKitAgentToolAction> MakeScanAssetsToolAction();
TSharedPtr<IHCIAbilityKitAgentToolAction> MakeScanMeshTriangleCountToolAction();
TSharedPtr<IHCIAbilityKitAgentToolAction> MakeSearchPathToolAction();
TSharedPtr<IHCIAbilityKitAgentToolAction> MakeScanLevelMeshRisksToolAction();
TSharedPtr<IHCIAbilityKitAgentToolAction> MakeSetTextureMaxSizeToolAction();
TSharedPtr<IHCIAbilityKitAgentToolAction> MakeSetMeshLODGroupToolAction();
TSharedPtr<IHCIAbilityKitAgentToolAction> MakeNormalizeAssetNamingByMetadataToolAction();
TSharedPtr<IHCIAbilityKitAgentToolAction> MakeAutoMaterialSetupByNameContractToolAction();
TSharedPtr<IHCIAbilityKitAgentToolAction> MakeRenameAssetToolAction();
TSharedPtr<IHCIAbilityKitAgentToolAction> MakeMoveAssetToolAction();
}
