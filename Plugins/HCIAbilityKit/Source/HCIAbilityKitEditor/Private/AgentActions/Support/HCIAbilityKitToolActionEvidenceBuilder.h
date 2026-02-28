#pragma once

#include "CoreMinimal.h"

#include "Agent/Tools/HCIAbilityKitAgentToolAction.h"

class FHCIAbilityKitToolActionEvidenceBuilder
{
public:
	static bool FailRequiredArgMissing(FHCIAbilityKitAgentToolActionResult& OutResult);

	static void SetFailed(
		FHCIAbilityKitAgentToolActionResult& OutResult,
		const TCHAR* ErrorCode,
		const TCHAR* Reason);

	static void SetSucceeded(
		FHCIAbilityKitAgentToolActionResult& OutResult,
		const TCHAR* Reason,
		int32 EstimatedAffectedCount = 0);

	static void AddEvidenceInt(
		FHCIAbilityKitAgentToolActionResult& OutResult,
		const TCHAR* Key,
		int32 Value);

	static void AddEvidenceBool(
		FHCIAbilityKitAgentToolActionResult& OutResult,
		const TCHAR* Key,
		bool bValue);

	static void AddEvidenceJoinedOrNone(
		FHCIAbilityKitAgentToolActionResult& OutResult,
		const TCHAR* Key,
		const TArray<FString>& Values,
		const TCHAR* Separator);

	static void AddEvidenceJoinedOrDash(
		FHCIAbilityKitAgentToolActionResult& OutResult,
		const TCHAR* Key,
		const TArray<FString>& Values,
		const TCHAR* Separator);
};

