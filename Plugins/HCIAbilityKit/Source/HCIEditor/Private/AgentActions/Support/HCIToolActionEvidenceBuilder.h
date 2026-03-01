#pragma once

#include "CoreMinimal.h"

#include "Agent/Tools/HCIAgentToolAction.h"

class FHCIToolActionEvidenceBuilder
{
public:
	static bool FailRequiredArgMissing(FHCIAgentToolActionResult& OutResult);

	static void SetFailed(
		FHCIAgentToolActionResult& OutResult,
		const TCHAR* ErrorCode,
		const TCHAR* Reason);

	static void SetSucceeded(
		FHCIAgentToolActionResult& OutResult,
		const TCHAR* Reason,
		int32 EstimatedAffectedCount = 0);

	static void AddEvidenceInt(
		FHCIAgentToolActionResult& OutResult,
		const TCHAR* Key,
		int32 Value);

	static void AddEvidenceBool(
		FHCIAgentToolActionResult& OutResult,
		const TCHAR* Key,
		bool bValue);

	static void AddEvidenceJoinedOrNone(
		FHCIAgentToolActionResult& OutResult,
		const TCHAR* Key,
		const TArray<FString>& Values,
		const TCHAR* Separator);

	static void AddEvidenceJoinedOrDash(
		FHCIAgentToolActionResult& OutResult,
		const TCHAR* Key,
		const TArray<FString>& Values,
		const TCHAR* Separator);
};


