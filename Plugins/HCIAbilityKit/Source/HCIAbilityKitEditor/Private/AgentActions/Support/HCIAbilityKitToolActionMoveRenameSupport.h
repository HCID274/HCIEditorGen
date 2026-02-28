#pragma once

#include "CoreMinimal.h"

#include "Agent/Tools/HCIAbilityKitAgentToolAction.h"

namespace HCIAbilityKitToolActionMoveRenameSupport
{
bool ValidateSourceAssetExists(const FString& SourceAssetPath, FHCIAbilityKitAgentToolActionResult& OutResult);
void FixupRedirectorReferencers(const FString& SourceAssetPath, FHCIAbilityKitAgentToolActionResult& OutResult);
bool MoveAssetWithAssetTools(const FString& SourceAssetPath, const FString& DestinationAssetPath);
}

