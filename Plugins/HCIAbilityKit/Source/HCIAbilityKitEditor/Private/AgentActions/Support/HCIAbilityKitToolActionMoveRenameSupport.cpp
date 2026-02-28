#include "AgentActions/Support/HCIAbilityKitToolActionMoveRenameSupport.h"

#include "AgentActions/Support/HCIAbilityKitAssetMoveRenameUtils.h"
#include "AgentActions/Support/HCIAbilityKitRedirectorFixup.h"

bool HCIAbilityKitToolActionMoveRenameSupport::ValidateSourceAssetExists(
	const FString& SourceAssetPath,
	FHCIAbilityKitAgentToolActionResult& OutResult)
{
	const HCIAbilityKitAssetMoveRenameUtils::FHCIAbilityKitValidateAssetExistsResult Check =
		HCIAbilityKitAssetMoveRenameUtils::ValidateSourceAssetExists(SourceAssetPath);
	if (!Check.bOk)
	{
		OutResult.bSucceeded = false;
		OutResult.ErrorCode = Check.ErrorCode.IsEmpty() ? TEXT("E4201") : Check.ErrorCode;
		OutResult.Reason = Check.Reason.IsEmpty() ? TEXT("asset_not_found") : Check.Reason;
		return false;
	}
	return true;
}

void HCIAbilityKitToolActionMoveRenameSupport::FixupRedirectorReferencers(
	const FString& SourceAssetPath,
	FHCIAbilityKitAgentToolActionResult& OutResult)
{
	const HCIAbilityKitRedirectorFixup::FHCIAbilityKitRedirectorFixupResult Fixup =
		HCIAbilityKitRedirectorFixup::FixupRedirectorReferencers(SourceAssetPath);
	OutResult.Evidence.Add(TEXT("redirector_fixup"), Fixup.Status.IsEmpty() ? TEXT("unknown") : Fixup.Status);
	if (Fixup.Status != TEXT("no_redirector_found"))
	{
		OutResult.Evidence.Add(TEXT("redirector_count"), FString::FromInt(Fixup.RedirectorCount));
	}
}

bool HCIAbilityKitToolActionMoveRenameSupport::MoveAssetWithAssetTools(
	const FString& SourceAssetPath,
	const FString& DestinationAssetPath)
{
	return HCIAbilityKitAssetMoveRenameUtils::MoveAssetWithAssetTools(SourceAssetPath, DestinationAssetPath);
}

