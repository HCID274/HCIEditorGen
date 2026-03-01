#include "Audit/HCIAuditRule.h"

void FHCIAuditIssue::AddEvidence(const FString& InKey, const FString& InValue)
{
	FHCIAuditEvidenceItem& Item = Evidence.Emplace_GetRef();
	Item.Key = InKey;
	Item.Value = InValue;
}


