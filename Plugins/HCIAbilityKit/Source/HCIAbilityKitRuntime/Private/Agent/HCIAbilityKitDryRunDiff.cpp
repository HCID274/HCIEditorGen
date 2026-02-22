#include "Agent/HCIAbilityKitDryRunDiff.h"

FString FHCIAbilityKitDryRunDiff::RiskToString(const EHCIAbilityKitDryRunRisk Risk)
{
	switch (Risk)
	{
	case EHCIAbilityKitDryRunRisk::ReadOnly:
		return TEXT("read_only");
	case EHCIAbilityKitDryRunRisk::Write:
		return TEXT("write");
	case EHCIAbilityKitDryRunRisk::Destructive:
		return TEXT("destructive");
	default:
		return TEXT("unknown");
	}
}

FString FHCIAbilityKitDryRunDiff::ObjectTypeToString(const EHCIAbilityKitDryRunObjectType ObjectType)
{
	switch (ObjectType)
	{
	case EHCIAbilityKitDryRunObjectType::Asset:
		return TEXT("asset");
	case EHCIAbilityKitDryRunObjectType::Actor:
		return TEXT("actor");
	default:
		return TEXT("unknown");
	}
}

FString FHCIAbilityKitDryRunDiff::LocateStrategyToString(const EHCIAbilityKitDryRunLocateStrategy LocateStrategy)
{
	switch (LocateStrategy)
	{
	case EHCIAbilityKitDryRunLocateStrategy::SyncBrowser:
		return TEXT("sync_browser");
	case EHCIAbilityKitDryRunLocateStrategy::CameraFocus:
		return TEXT("camera_focus");
	default:
		return TEXT("unknown");
	}
}

void FHCIAbilityKitDryRunDiff::NormalizeAndFinalize(FHCIAbilityKitDryRunDiffReport& InOutReport)
{
	InOutReport.Summary.TotalCandidates = InOutReport.DiffItems.Num();
	InOutReport.Summary.Modifiable = 0;
	InOutReport.Summary.Skipped = 0;

	for (FHCIAbilityKitDryRunDiffItem& Item : InOutReport.DiffItems)
	{
		if (Item.ObjectType == EHCIAbilityKitDryRunObjectType::Actor)
		{
			Item.LocateStrategy = EHCIAbilityKitDryRunLocateStrategy::CameraFocus;
			if (Item.EvidenceKey.IsEmpty())
			{
				Item.EvidenceKey = TEXT("actor_path");
			}
		}
		else
		{
			Item.LocateStrategy = EHCIAbilityKitDryRunLocateStrategy::SyncBrowser;
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

