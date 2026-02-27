#pragma once

#include "CoreMinimal.h"
#include "Agent/Planner/HCIAbilityKitAgentPlan.h"
#include "Agent/Planner/HCIAbilityKitAgentPlanner.h"
#include "UObject/Object.h"
#include "HCIAbilityKitAgentCommandBase.generated.h"

struct FHCIAbilityKitAgentCommandContext
{
	FString InputParam;
	FString SourceTag;
	// Optional extra ENV_CONTEXT injected into planner prompt. Intended for structured, non-chat signals
	// (e.g. latest external ingest batch manifest summary).
	FString ExtraEnvContextText;
};

struct FHCIAbilityKitAgentCommandResult
{
	bool bSuccess = false;
	bool bSummaryFailure = false;
	FString Message;
	bool bHasPlanPayload = false;
	FHCIAbilityKitAgentPlan Plan;
	FString RouteReason;
	FHCIAbilityKitAgentPlannerResultMetadata PlannerMetadata;
};

DECLARE_DELEGATE_OneParam(FHCIAbilityKitAgentCommandComplete, const FHCIAbilityKitAgentCommandResult&);

/**
 * Agent 命令基类：封装具体业务执行，UI 不直接依赖业务实现。
 */
UCLASS(Abstract)
class HCIABILITYKITEDITOR_API UHCIAbilityKitAgentCommandBase : public UObject
{
	GENERATED_BODY()

public:
	virtual void ExecuteAsync(
		const FHCIAbilityKitAgentCommandContext& Context,
		FHCIAbilityKitAgentCommandComplete&& OnComplete) PURE_VIRTUAL(UHCIAbilityKitAgentCommandBase::ExecuteAsync, );
};
