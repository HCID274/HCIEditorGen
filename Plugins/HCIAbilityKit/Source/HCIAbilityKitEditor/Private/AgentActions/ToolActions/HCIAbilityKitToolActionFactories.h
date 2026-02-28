#pragma once

#include "CoreMinimal.h"

#include "Agent/Tools/HCIAbilityKitAgentToolAction.h"

namespace HCIAbilityKitToolActionFactories
{
TSharedPtr<IHCIAbilityKitAgentToolAction> MakeScanAssetsToolAction();
TSharedPtr<IHCIAbilityKitAgentToolAction> MakeScanMeshTriangleCountToolAction();
TSharedPtr<IHCIAbilityKitAgentToolAction> MakeScanLevelMeshRisksToolAction();
TSharedPtr<IHCIAbilityKitAgentToolAction> MakeSetTextureMaxSizeToolAction();
TSharedPtr<IHCIAbilityKitAgentToolAction> MakeSetMeshLODGroupToolAction();
TSharedPtr<IHCIAbilityKitAgentToolAction> MakeNormalizeAssetNamingByMetadataToolAction();
}

