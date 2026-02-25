#pragma once

#include "CoreMinimal.h"

struct FHCIAbilityKitAgentApplyConfirmRequest;
struct FHCIAbilityKitAgentApplyRequest;
struct FHCIAbilityKitAgentExecuteTicket;
struct FHCIAbilityKitAgentSimulateExecuteFinalReport;
struct FHCIAbilityKitAgentSimulateExecuteReceipt;
struct FHCIAbilityKitDryRunDiffReport;

class HCIABILITYKITRUNTIME_API FHCIAbilityKitAgentExecutorSimulateExecuteFinalReportBridge
{
public:
	static bool BuildSimulateExecuteFinalReport(
		const FHCIAbilityKitAgentSimulateExecuteReceipt& SimExecuteReceipt,
		const FHCIAbilityKitAgentExecuteTicket& CurrentExecuteTicket,
		const FHCIAbilityKitAgentApplyConfirmRequest& CurrentConfirmRequest,
		const FHCIAbilityKitAgentApplyRequest& CurrentApplyRequest,
		const FHCIAbilityKitDryRunDiffReport& CurrentReviewReport,
		FHCIAbilityKitAgentSimulateExecuteFinalReport& OutReport);
};
