#pragma once

#include "CoreMinimal.h"
#include "Agent/Planner/HCIAgentPlan.h"
#include "Agent/Planner/HCIAgentPlanner.h"
#include "UObject/Object.h"
#include "HCIAgentCommandBase.generated.h"

struct FHCIAgentCommandContext
{
	FString InputParam;
	FString SourceTag;
	// Optional extra ENV_CONTEXT injected into planner prompt. Intended for structured, non-chat signals
	// (e.g. latest external ingest batch manifest summary).
	FString ExtraEnvContextText;
};

struct FHCIAgentCommandResult
{
	bool bSuccess = false;
	bool bSummaryFailure = false;
	FString Message;
	bool bHasPlanPayload = false;
	FHCIAgentPlan Plan;
	FString RouteReason;
	FHCIAgentPlannerResultMetadata PlannerMetadata;
};

DECLARE_DELEGATE_OneParam(FHCIAgentCommandComplete, const FHCIAgentCommandResult&);

/**
 * Agent 命令基类：封装具体业务执行，UI 不直接依赖业务实现。
 */
UCLASS(Abstract)
class HCIEDITOR_API UHCIAgentCommandBase : public UObject
{
	GENERATED_BODY()

public:
	virtual void ExecuteAsync(
		const FHCIAgentCommandContext& Context,
		FHCIAgentCommandComplete&& OnComplete) PURE_VIRTUAL(UHCIAgentCommandBase::ExecuteAsync, );
};


