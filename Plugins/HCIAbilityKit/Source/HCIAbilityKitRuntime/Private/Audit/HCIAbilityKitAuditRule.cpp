#include "Audit/HCIAbilityKitAuditRule.h"

void FHCIAbilityKitAuditIssue::AddEvidence(const FString& InKey, const FString& InValue)
{
	FHCIAbilityKitAuditEvidenceItem& Item = Evidence.Emplace_GetRef();
	Item.Key = InKey;
	Item.Value = InValue;
}

