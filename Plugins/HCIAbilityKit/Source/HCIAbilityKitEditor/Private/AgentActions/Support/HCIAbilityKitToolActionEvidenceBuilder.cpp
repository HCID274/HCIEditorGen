#include "AgentActions/Support/HCIAbilityKitToolActionEvidenceBuilder.h"

bool FHCIAbilityKitToolActionEvidenceBuilder::FailRequiredArgMissing(FHCIAbilityKitAgentToolActionResult& OutResult)
{
	OutResult = FHCIAbilityKitAgentToolActionResult();
	OutResult.bSucceeded = false;
	OutResult.ErrorCode = TEXT("E4001");
	OutResult.Reason = TEXT("required_arg_missing");
	return false;
}

void FHCIAbilityKitToolActionEvidenceBuilder::SetFailed(
	FHCIAbilityKitAgentToolActionResult& OutResult,
	const TCHAR* ErrorCode,
	const TCHAR* Reason)
{
	OutResult = FHCIAbilityKitAgentToolActionResult();
	OutResult.bSucceeded = false;
	OutResult.ErrorCode = ErrorCode;
	OutResult.Reason = Reason;
}

void FHCIAbilityKitToolActionEvidenceBuilder::SetSucceeded(
	FHCIAbilityKitAgentToolActionResult& OutResult,
	const TCHAR* Reason,
	int32 EstimatedAffectedCount)
{
	OutResult = FHCIAbilityKitAgentToolActionResult();
	OutResult.bSucceeded = true;
	OutResult.Reason = Reason;
	OutResult.EstimatedAffectedCount = EstimatedAffectedCount;
}

void FHCIAbilityKitToolActionEvidenceBuilder::AddEvidenceInt(
	FHCIAbilityKitAgentToolActionResult& OutResult,
	const TCHAR* Key,
	int32 Value)
{
	OutResult.Evidence.Add(Key, FString::FromInt(Value));
}

void FHCIAbilityKitToolActionEvidenceBuilder::AddEvidenceBool(
	FHCIAbilityKitAgentToolActionResult& OutResult,
	const TCHAR* Key,
	bool bValue)
{
	OutResult.Evidence.Add(Key, bValue ? TEXT("true") : TEXT("false"));
}

void FHCIAbilityKitToolActionEvidenceBuilder::AddEvidenceJoinedOrNone(
	FHCIAbilityKitAgentToolActionResult& OutResult,
	const TCHAR* Key,
	const TArray<FString>& Values,
	const TCHAR* Separator)
{
	OutResult.Evidence.Add(Key, Values.Num() > 0 ? FString::Join(Values, Separator) : TEXT("none"));
}

void FHCIAbilityKitToolActionEvidenceBuilder::AddEvidenceJoinedOrDash(
	FHCIAbilityKitAgentToolActionResult& OutResult,
	const TCHAR* Key,
	const TArray<FString>& Values,
	const TCHAR* Separator)
{
	OutResult.Evidence.Add(Key, Values.Num() > 0 ? FString::Join(Values, Separator) : TEXT("-"));
}

