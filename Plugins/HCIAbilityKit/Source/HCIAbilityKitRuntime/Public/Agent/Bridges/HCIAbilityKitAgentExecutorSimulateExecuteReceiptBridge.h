#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentApplyConfirmRequest;
struct FHCIAbilityKitAgentApplyRequest;
struct FHCIAbilityKitAgentExecuteTicket;
struct FHCIAbilityKitAgentSimulateExecuteReceipt;
struct FHCIAbilityKitDryRunDiffReport;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentExecutorSimulateExecuteReceiptBridge
{
public:
	static bool BuildSimulateExecuteReceipt(
		const FHCIAbilityKitAgentExecuteTicket& ExecuteTicket,
		const FHCIAbilityKitAgentApplyConfirmRequest& CurrentConfirmRequest,
		const FHCIAbilityKitAgentApplyRequest& CurrentApplyRequest,
		const FHCIAbilityKitDryRunDiffReport& CurrentReviewReport,
		FHCIAbilityKitAgentSimulateExecuteReceipt& OutReceipt);
};
