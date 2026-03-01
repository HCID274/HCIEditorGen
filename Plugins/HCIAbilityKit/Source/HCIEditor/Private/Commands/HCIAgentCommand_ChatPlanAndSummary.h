#pragma once

#include "Commands/HCIAgentCommandBase.h"
#include "HCIAgentCommand_ChatPlanAndSummary.generated.h"

/**
 * 聊天提交命令：执行计划预览并请求摘要。
 */
UCLASS()
class UHCIAgentCommand_ChatPlanAndSummary : public UHCIAgentCommandBase
{
	GENERATED_BODY()

public:
	virtual void ExecuteAsync(
		const FHCIAgentCommandContext& Context,
		FHCIAgentCommandComplete&& OnComplete) override;
};

