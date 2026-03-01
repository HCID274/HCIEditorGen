#include "AgentActions/Support/HCIToolActionEvidenceBuilder.h"

bool FHCIToolActionEvidenceBuilder::FailRequiredArgMissing(FHCIAgentToolActionResult& OutResult)
{
	OutResult = FHCIAgentToolActionResult();
	OutResult.bSucceeded = false;
	OutResult.ErrorCode = TEXT("E4001");
	OutResult.Reason = TEXT("required_arg_missing");
	return false;
}

void FHCIToolActionEvidenceBuilder::SetFailed(
	FHCIAgentToolActionResult& OutResult,
	const TCHAR* ErrorCode,
	const TCHAR* Reason)
{
	OutResult = FHCIAgentToolActionResult();
	OutResult.bSucceeded = false;
	OutResult.ErrorCode = ErrorCode;
	OutResult.Reason = Reason;
}

void FHCIToolActionEvidenceBuilder::SetSucceeded(
	FHCIAgentToolActionResult& OutResult,
	const TCHAR* Reason,
	int32 EstimatedAffectedCount)
{
	OutResult = FHCIAgentToolActionResult();
	OutResult.bSucceeded = true;
	OutResult.Reason = Reason;
	OutResult.EstimatedAffectedCount = EstimatedAffectedCount;
}

void FHCIToolActionEvidenceBuilder::AddEvidenceInt(
	FHCIAgentToolActionResult& OutResult,
	const TCHAR* Key,
	int32 Value)
{
	OutResult.Evidence.Add(Key, FString::FromInt(Value));
}

void FHCIToolActionEvidenceBuilder::AddEvidenceBool(
	FHCIAgentToolActionResult& OutResult,
	const TCHAR* Key,
	bool bValue)
{
	OutResult.Evidence.Add(Key, bValue ? TEXT("true") : TEXT("false"));
}

void FHCIToolActionEvidenceBuilder::AddEvidenceJoinedOrNone(
	FHCIAgentToolActionResult& OutResult,
	const TCHAR* Key,
	const TArray<FString>& Values,
	const TCHAR* Separator)
{
	OutResult.Evidence.Add(Key, Values.Num() > 0 ? FString::Join(Values, Separator) : TEXT("none"));
}

void FHCIToolActionEvidenceBuilder::AddEvidenceJoinedOrDash(
	FHCIAgentToolActionResult& OutResult,
	const TCHAR* Key,
	const TArray<FString>& Values,
	const TCHAR* Separator)
{
	OutResult.Evidence.Add(Key, Values.Num() > 0 ? FString::Join(Values, Separator) : TEXT("-"));
}


