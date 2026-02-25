#include "Agent/Bridges/HCIAbilityKitAgentExecutorDryRunBridge.h"

#include "Agent/Executor/HCIAbilityKitAgentExecutor.h"
#include "Agent/Executor/HCIAbilityKitDryRunDiff.h"

namespace
{
static EHCIAbilityKitDryRunRisk HCI_MapRiskLevelToDryRunRisk(const FString& InRiskLevel)
{
	const FString RiskLower = InRiskLevel.ToLower();
	if (RiskLower == TEXT("destructive"))
	{
		return EHCIAbilityKitDryRunRisk::Destructive;
	}
	if (RiskLower == TEXT("write"))
	{
		return EHCIAbilityKitDryRunRisk::Write;
	}
	return EHCIAbilityKitDryRunRisk::ReadOnly;
}

static FString HCI_GetMapValueOrDefault(const TMap<FString, FString>& Map, const FString& Key, const FString& DefaultValue = TEXT(""))
{
	if (const FString* Found = Map.Find(Key))
	{
		return *Found;
	}
	return DefaultValue;
}

static FString HCI_BuildFailureSkipReason(const FHCIAbilityKitAgentExecutorStepResult& StepResult)
{
	TArray<FString> Parts;
	if (!StepResult.ErrorCode.IsEmpty())
	{
		Parts.Add(FString::Printf(TEXT("error_code=%s"), *StepResult.ErrorCode));
	}
	if (!StepResult.FailurePhase.IsEmpty() && StepResult.FailurePhase != TEXT("-"))
	{
		Parts.Add(FString::Printf(TEXT("failure_phase=%s"), *StepResult.FailurePhase));
	}
	if (!StepResult.PreflightGate.IsEmpty() && StepResult.PreflightGate != TEXT("-"))
	{
		Parts.Add(FString::Printf(TEXT("preflight_gate=%s"), *StepResult.PreflightGate));
	}
	if (!StepResult.Reason.IsEmpty())
	{
		Parts.Add(FString::Printf(TEXT("reason=%s"), *StepResult.Reason));
	}

	if (Parts.Num() == 0)
	{
		return StepResult.Status.IsEmpty() ? TEXT("executor_row_unspecified") : StepResult.Status;
	}
	return FString::Join(Parts, TEXT(";"));
}
}

bool FHCIAbilityKitAgentExecutorDryRunBridge::BuildDryRunDiffReport(
	const FHCIAbilityKitAgentExecutorRunResult& RunResult,
	FHCIAbilityKitDryRunDiffReport& OutReport)
{
	OutReport = FHCIAbilityKitDryRunDiffReport();
	OutReport.RequestId = RunResult.RequestId;
	OutReport.DiffItems.Reserve(RunResult.StepResults.Num());

	for (const FHCIAbilityKitAgentExecutorStepResult& StepResult : RunResult.StepResults)
	{
		FHCIAbilityKitDryRunDiffItem& Item = OutReport.DiffItems.AddDefaulted_GetRef();

		Item.ToolName = StepResult.ToolName;
		Item.Risk = HCI_MapRiskLevelToDryRunRisk(StepResult.RiskLevel);
		Item.Field = StepResult.StepId.IsEmpty()
						 ? TEXT("executor_step_status")
						 : FString::Printf(TEXT("step:%s"), *StepResult.StepId);

		const FString ActorPath = HCI_GetMapValueOrDefault(StepResult.Evidence, TEXT("actor_path"));
		const FString AssetPath = HCI_GetMapValueOrDefault(StepResult.Evidence, TEXT("asset_path"));
		if (!ActorPath.IsEmpty())
		{
			Item.AssetPath = ActorPath;
			Item.ActorPath = ActorPath;
			Item.ObjectType = EHCIAbilityKitDryRunObjectType::Actor;
			Item.EvidenceKey = TEXT("actor_path");
		}
		else
		{
			Item.AssetPath = AssetPath.IsEmpty() ? TEXT("-") : AssetPath;
			Item.ObjectType = EHCIAbilityKitDryRunObjectType::Asset;
			Item.EvidenceKey = AssetPath.IsEmpty() ? TEXT("result") : TEXT("asset_path");
		}

		const FString Before = HCI_GetMapValueOrDefault(StepResult.Evidence, TEXT("before"));
		const FString After = HCI_GetMapValueOrDefault(StepResult.Evidence, TEXT("after"));
		Item.Before = Before.IsEmpty() ? TEXT("-") : Before;
		if (!After.IsEmpty())
		{
			Item.After = After;
		}
		else if (!StepResult.Status.IsEmpty())
		{
			Item.After = StepResult.Status;
		}
		else
		{
			Item.After = TEXT("-");
		}

		if (StepResult.Status == TEXT("failed") || StepResult.Status == TEXT("skipped"))
		{
			Item.SkipReason = HCI_BuildFailureSkipReason(StepResult);
		}
	}

	FHCIAbilityKitDryRunDiff::NormalizeAndFinalize(OutReport);
	return true;
}
