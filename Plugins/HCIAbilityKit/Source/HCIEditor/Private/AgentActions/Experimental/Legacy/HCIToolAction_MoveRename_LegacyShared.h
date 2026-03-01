#pragma once

#include "CoreMinimal.h"

// Legacy snapshot (Global Step 2 refactor): kept for reference only; do not include from production ToolActions.

#include "Agent/Tools/HCIAgentToolAction.h"
#include "AgentActions/Support/HCIAssetMoveRenameUtils.h"
#include "AgentActions/Support/HCIRedirectorFixup.h"

namespace
{
static bool HCI_ValidateSourceAssetExists(
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

static void HCI_FixupRedirectorReferencers(
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

static bool HCI_MoveAssetWithAssetTools(const FString& SourceAssetPath, const FString& DestinationAssetPath)
{
	return HCIAssetMoveRenameUtils::MoveAssetWithAssetTools(SourceAssetPath, DestinationAssetPath);
}
}

