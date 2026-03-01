#include "AgentActions/Support/HCIToolActionMoveRenameSupport.h"

#include "AgentActions/Support/HCIAssetMoveRenameUtils.h"
#include "AgentActions/Support/HCIRedirectorFixup.h"

bool HCIToolActionMoveRenameSupport::ValidateSourceAssetExists(
	const FString& SourceAssetPath,
	FHCIAgentToolActionResult& OutResult)
{
	const HCIAssetMoveRenameUtils::FHCIValidateAssetExistsResult Check =
		HCIAssetMoveRenameUtils::ValidateSourceAssetExists(SourceAssetPath);
	if (!Check.bOk)
	{
		OutResult.bSucceeded = false;
		OutResult.ErrorCode = Check.ErrorCode.IsEmpty() ? TEXT("E4201") : Check.ErrorCode;
		OutResult.Reason = Check.Reason.IsEmpty() ? TEXT("asset_not_found") : Check.Reason;
		return false;
	}
	return true;
}

void HCIToolActionMoveRenameSupport::FixupRedirectorReferencers(
	const FString& SourceAssetPath,
	FHCIAgentToolActionResult& OutResult)
{
	const HCIRedirectorFixup::FHCIRedirectorFixupResult Fixup =
		HCIRedirectorFixup::FixupRedirectorReferencers(SourceAssetPath);
	OutResult.Evidence.Add(TEXT("redirector_fixup"), Fixup.Status.IsEmpty() ? TEXT("unknown") : Fixup.Status);
	if (Fixup.Status != TEXT("no_redirector_found"))
	{
		OutResult.Evidence.Add(TEXT("redirector_count"), FString::FromInt(Fixup.RedirectorCount));
	}
}

bool HCIToolActionMoveRenameSupport::MoveAssetWithAssetTools(
	const FString& SourceAssetPath,
	const FString& DestinationAssetPath)
{
	return HCIAssetMoveRenameUtils::MoveAssetWithAssetTools(SourceAssetPath, DestinationAssetPath);
}


