#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HCIAbilityKitAgentCommandBase.generated.h"

struct FHCIAbilityKitAgentCommandContext
{
	FString InputParam;
	FString SourceTag;
};

struct FHCIAbilityKitAgentCommandResult
{
	bool bSuccess = false;
	bool bSummaryFailure = false;
	FString Message;
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
