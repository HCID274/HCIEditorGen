#include "Agent/Executor/HCIDryRunDiff.h"

FString FHCIDryRunDiff::RiskToString(const EHCIDryRunRisk Risk)
{
	switch (Risk)
	{
	case EHCIDryRunRisk::ReadOnly:
		return TEXT("read_only");
	case EHCIDryRunRisk::Write:
		return TEXT("write");
	case EHCIDryRunRisk::Destructive:
		return TEXT("destructive");
	default:
		return TEXT("unknown");
	}
}

FString FHCIDryRunDiff::ObjectTypeToString(const EHCIDryRunObjectType ObjectType)
{
	switch (ObjectType)
	{
	case EHCIDryRunObjectType::Asset:
		return TEXT("asset");
	case EHCIDryRunObjectType::Actor:
		return TEXT("actor");
	default:
		return TEXT("unknown");
	}
}

FString FHCIDryRunDiff::LocateStrategyToString(const EHCIDryRunLocateStrategy LocateStrategy)
{
	switch (LocateStrategy)
	{
	case EHCIDryRunLocateStrategy::SyncBrowser:
		return TEXT("sync_browser");
	case EHCIDryRunLocateStrategy::CameraFocus:
		return TEXT("camera_focus");
	default:
		return TEXT("unknown");
	}
}

void FHCIDryRunDiff::NormalizeAndFinalize(FHCIDryRunDiffReport& InOutReport)
{
	InOutReport.Summary.TotalCandidates = InOutReport.DiffItems.Num();
	InOutReport.Summary.Modifiable = 0;
	InOutReport.Summary.Skipped = 0;

	for (FHCIDryRunDiffItem& Item : InOutReport.DiffItems)
	{
		if (Item.ObjectType == EHCIDryRunObjectType::Actor)
		{
			Item.LocateStrategy = EHCIDryRunLocateStrategy::CameraFocus;
			if (Item.EvidenceKey.IsEmpty())
			{
				Item.EvidenceKey = TEXT("actor_path");
			}
		}
		else
		{
			Item.LocateStrategy = EHCIDryRunLocateStrategy::SyncBrowser;
			if (Item.EvidenceKey.IsEmpty())
			{
				Item.EvidenceKey = TEXT("asset_path");
			}
		}

		if (Item.SkipReason.IsEmpty())
		{
			++InOutReport.Summary.Modifiable;
		}
		else
		{
			++InOutReport.Summary.Skipped;
		}
	}
}

