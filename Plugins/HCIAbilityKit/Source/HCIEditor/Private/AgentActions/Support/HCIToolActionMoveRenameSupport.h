#pragma once

#include "CoreMinimal.h"

#include "Agent/Tools/HCIAgentToolAction.h"

namespace HCIToolActionMoveRenameSupport
{
bool ValidateSourceAssetExists(const FString& SourceAssetPath, FHCIAgentToolActionResult& OutResult);
void FixupRedirectorReferencers(const FString& SourceAssetPath, FHCIAgentToolActionResult& OutResult);
bool MoveAssetWithAssetTools(const FString& SourceAssetPath, const FString& DestinationAssetPath);
}


