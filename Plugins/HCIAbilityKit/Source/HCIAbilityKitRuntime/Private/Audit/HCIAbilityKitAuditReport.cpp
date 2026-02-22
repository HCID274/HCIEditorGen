#include "Audit/HCIAbilityKitAuditReport.h"

#include "Audit/HCIAbilityKitAuditScanService.h"

namespace
{
static FString BuildDefaultRunId(const FDateTime& GeneratedUtc)
{
	const FDateTime SafeTime = GeneratedUtc.GetTicks() > 0 ? GeneratedUtc : FDateTime::UtcNow();
	return FString::Printf(
		TEXT("audit_%04d%02d%02d_%02d%02d%02d"),
		SafeTime.GetYear(),
		SafeTime.GetMonth(),
		SafeTime.GetDay(),
		SafeTime.GetHour(),
		SafeTime.GetMinute(),
		SafeTime.GetSecond());
}
}

FString FHCIAbilityKitAuditReportBuilder::SeverityToString(const EHCIAbilityKitAuditSeverity Severity)
{
	switch (Severity)
	{
	case EHCIAbilityKitAuditSeverity::Info:
		return TEXT("Info");
	case EHCIAbilityKitAuditSeverity::Warn:
		return TEXT("Warn");
	case EHCIAbilityKitAuditSeverity::Error:
		return TEXT("Error");
	default:
		return TEXT("Info");
	}
}

FHCIAbilityKitAuditReport FHCIAbilityKitAuditReportBuilder::BuildFromSnapshot(
	const FHCIAbilityKitAuditScanSnapshot& Snapshot,
	const FString& RunIdOverride)
{
	FHCIAbilityKitAuditReport Report;
	Report.GeneratedUtc = Snapshot.Stats.UpdatedUtc.GetTicks() > 0 ? Snapshot.Stats.UpdatedUtc : FDateTime::UtcNow();
	Report.Source = Snapshot.Stats.Source;
	Report.RunId = RunIdOverride.IsEmpty() ? BuildDefaultRunId(Report.GeneratedUtc) : RunIdOverride;

	int32 TotalIssueCount = 0;
	for (const FHCIAbilityKitAuditAssetRow& Row : Snapshot.Rows)
	{
		TotalIssueCount += Row.AuditIssues.Num();
	}
	Report.Results.Reserve(TotalIssueCount);

	for (const FHCIAbilityKitAuditAssetRow& Row : Snapshot.Rows)
	{
		for (const FHCIAbilityKitAuditIssue& Issue : Row.AuditIssues)
		{
			FHCIAbilityKitAuditResultEntry& Entry = Report.Results.Emplace_GetRef();
			Entry.AssetPath = Row.AssetPath;
			Entry.AssetName = Row.AssetName;
			Entry.AssetClass = Row.AssetClass;
			Entry.RuleId = Issue.RuleId.ToString();
			Entry.Severity = Issue.Severity;
			Entry.SeverityText = SeverityToString(Issue.Severity);
			Entry.Reason = Issue.Reason;
			Entry.Hint = Issue.Hint;
			Entry.TriangleSource = Row.TriangleSource;
			Entry.ScanState = Row.ScanState;
			Entry.SkipReason = Row.SkipReason;

			for (const FHCIAbilityKitAuditEvidenceItem& EvidenceItem : Issue.Evidence)
			{
				Entry.Evidence.Add(EvidenceItem.Key, EvidenceItem.Value);
			}

			// Ensure core trace fields exist even if a rule omits them.
			Entry.Evidence.FindOrAdd(TEXT("scan_state")) = Row.ScanState;
			if (!Row.TriangleSource.IsEmpty())
			{
				Entry.Evidence.FindOrAdd(TEXT("triangle_source")) = Row.TriangleSource;
			}
			if (!Row.RepresentingMeshPath.IsEmpty())
			{
				Entry.Evidence.FindOrAdd(TEXT("representing_mesh_path")) = Row.RepresentingMeshPath;
			}
		}
	}

	return Report;
}

