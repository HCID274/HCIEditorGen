#pragma once

#include "Commands/HCIAbilityKitAgentCommandBase.h"
#include "HCIAbilityKitAgentCommand_ChatPlanAndSummary.generated.h"

/**
 * 聊天提交命令：执行计划预览并请求摘要。
 */
UCLASS()
class UHCIAbilityKitAgentCommand_ChatPlanAndSummary : public UHCIAbilityKitAgentCommandBase
{
	GENERATED_BODY()

public:
	virtual void ExecuteAsync(
		const FHCIAbilityKitAgentCommandContext& Context,
		FHCIAbilityKitAgentCommandComplete&& OnComplete) override;
};
