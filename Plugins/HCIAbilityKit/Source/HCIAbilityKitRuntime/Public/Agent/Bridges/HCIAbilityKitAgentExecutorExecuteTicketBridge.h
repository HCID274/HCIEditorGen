#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentApplyConfirmRequest;
struct FHCIAbilityKitAgentApplyRequest;
struct FHCIAbilityKitAgentExecuteTicket;
struct FHCIAbilityKitDryRunDiffReport;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentExecutorExecuteTicketBridge
{
public:
	static bool BuildExecuteTicket(
		const FHCIAbilityKitAgentApplyConfirmRequest& ConfirmRequest,
		const FHCIAbilityKitAgentApplyRequest& CurrentApplyRequest,
		const FHCIAbilityKitDryRunDiffReport& CurrentReviewReport,
		FHCIAbilityKitAgentExecuteTicket& OutExecuteTicket);
};

