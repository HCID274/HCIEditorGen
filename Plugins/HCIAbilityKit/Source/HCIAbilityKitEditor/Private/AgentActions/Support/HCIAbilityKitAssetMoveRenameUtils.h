#pragma once

#include "CoreMinimal.h"

namespace HCIAbilityKitAssetMoveRenameUtils
{
struct FHCIAbilityKitValidateAssetExistsResult
{
	bool bOk = false;
	FString ErrorCode;
	FString Reason;
};

FHCIAbilityKitValidateAssetExistsResult ValidateSourceAssetExists(const FString& SourceAssetPath);

bool MoveAssetWithAssetTools(const FString& SourceAssetPath, const FString& DestinationAssetPath);
}

