#pragma once

#include "CoreMinimal.h"

namespace HCIAssetMoveRenameUtils
{
struct FHCIValidateAssetExistsResult
{
	bool bOk = false;
	FString ErrorCode;
	FString Reason;
};

FHCIValidateAssetExistsResult ValidateSourceAssetExists(const FString& SourceAssetPath);

bool MoveAssetWithAssetTools(const FString& SourceAssetPath, const FString& DestinationAssetPath);
}


