#include "AgentActions/ToolActions/HCIToolActionFactories.h"

#include "AgentActions/Support/HCIToolActionEvidenceBuilder.h"
#include "AgentActions/Support/HCIToolActionParamParser.h"

#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"
#include "PhysicsEngine/BodySetup.h"

namespace
{
class FHCIScanLevelMeshRisksToolAction final : public IHCIAgentToolAction
{
public:
	virtual FName GetToolName() const override
	{
		return TEXT("ScanLevelMeshRisks");
	}

	virtual bool DryRun(
		const FHCIAgentToolActionRequest& Request,
		FHCIAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult);
	}

	virtual bool Execute(
		const FHCIAgentToolActionRequest& Request,
		FHCIAgentToolActionResult& OutResult) const override
	{
		return RunInternal(Request, OutResult);
	}

private:
	static bool RunInternal(
		const FHCIAgentToolActionRequest& Request,
		FHCIAgentToolActionResult& OutResult)
	{
		FString Scope;
		TArray<FString> Checks;
		TArray<FString> RequestedActorNames;
		int32 MaxActorCount = 0;
		const FHCIToolActionParamParser Params(Request.Args);
		if (!Params.TryGetRequiredString(TEXT("scope"), Scope) ||
			!Params.TryGetRequiredStringArray(TEXT("checks"), Checks) ||
			!Params.TryGetRequiredInt(TEXT("max_actor_count"), MaxActorCount))
		{
			return FHCIToolActionEvidenceBuilder::FailRequiredArgMissing(OutResult);
		}
		if (!Params.TryGetOptionalStringArray(TEXT("actor_names"), RequestedActorNames))
		{
			OutResult = FHCIAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4002");
			OutResult.Reason = TEXT("invalid_actor_names");
			return false;
		}

		if (Scope != TEXT("selected") && Scope != TEXT("all"))
		{
			OutResult = FHCIAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4002");
			OutResult.Reason = TEXT("invalid_scope");
			return false;
		}

		UWorld* World = nullptr;
		if (GEditor)
		{
			World = GEditor->GetEditorWorldContext().World();
		}
		if (!World)
		{
			World = GWorld;
		}
		if (!World)
		{
			OutResult = FHCIAgentToolActionResult();
			OutResult.bSucceeded = false;
			OutResult.ErrorCode = TEXT("E4205");
			OutResult.Reason = TEXT("no_editor_world");
			return false;
		}

		TArray<AActor*> ActorsToScan;
		if (Scope == TEXT("selected"))
		{
			USelection* SelectedActors = GEditor ? GEditor->GetSelectedActors() : nullptr;
			if (SelectedActors)
			{
				for (FSelectionIterator It(*SelectedActors); It; ++It)
				{
					if (AActor* Actor = Cast<AActor>(*It))
					{
						ActorsToScan.Add(Actor);
					}
				}
			}

			if (ActorsToScan.Num() == 0)
			{
				OutResult = FHCIAgentToolActionResult();
				OutResult.bSucceeded = false;
				OutResult.ErrorCode = TEXT("no_actors_selected");
				OutResult.Reason = TEXT("no_actors_selected");
				OutResult.Evidence.Add(TEXT("scope"), Scope);
				OutResult.Evidence.Add(TEXT("selected_actor_count"), TEXT("0"));
				// Keep evidence keys stable even on failure, so UI stats don't fall back to TargetCountEstimate.
				OutResult.Evidence.Add(TEXT("candidate_actor_count"), TEXT("0"));
				OutResult.Evidence.Add(TEXT("scanned_count"), TEXT("0"));
				OutResult.Evidence.Add(TEXT("risky_count"), TEXT("0"));
				OutResult.Evidence.Add(TEXT("risk_summary"), TEXT("No actor selected. Select one or switch scope to 'all'."));
				OutResult.Evidence.Add(TEXT("risky_actors"), TEXT("none"));
				OutResult.Evidence.Add(TEXT("missing_collision_actors"), TEXT("none"));
				OutResult.Evidence.Add(TEXT("default_material_actors"), TEXT("none"));
				OutResult.Evidence.Add(TEXT("result"), TEXT("no_actors_selected"));
				return false;
			}
		}
		else // "all"
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				ActorsToScan.Add(*It);
			}
		}

		if (RequestedActorNames.Num() > 0)
		{
			TArray<AActor*> FilteredActors;
			FilteredActors.Reserve(ActorsToScan.Num());
			for (AActor* Actor : ActorsToScan)
			{
				if (!Actor)
				{
					continue;
				}

				const FString ActorLabel = Actor->GetActorLabel();
				const FString ActorName = Actor->GetName();
				const FString ActorPath = Actor->GetPathName();
				const bool bMatched = RequestedActorNames.ContainsByPredicate(
					[&ActorLabel, &ActorName, &ActorPath](const FString& RequestedName)
					{
						return ActorLabel.Equals(RequestedName, ESearchCase::IgnoreCase) ||
							ActorName.Equals(RequestedName, ESearchCase::IgnoreCase) ||
							ActorPath.Equals(RequestedName, ESearchCase::IgnoreCase);
					});
				if (bMatched)
				{
					FilteredActors.Add(Actor);
				}
			}
			ActorsToScan = MoveTemp(FilteredActors);
		}

		const bool bCheckMissingCollision = Checks.Contains(TEXT("missing_collision"));
		const bool bCheckDefaultMaterial = Checks.Contains(TEXT("default_material"));

		TArray<FString> RiskyActorNames;
		TArray<FString> RiskyActorPaths;
		TArray<FString> RiskIssueDetails;
		TArray<FString> MissingCollisionActors;
		TArray<FString> DefaultMaterialActors;
		int32 ScannedCount = 0;

		for (AActor* Actor : ActorsToScan)
		{
			if (MaxActorCount > 0 && ScannedCount >= MaxActorCount)
			{
				break;
			}

			TArray<UStaticMeshComponent*> MeshComponents;
			Actor->GetComponents<UStaticMeshComponent>(MeshComponents);
			if (MeshComponents.Num() == 0)
			{
				continue;
			}

			bool bHasValidStaticMeshComponent = false;
			for (UStaticMeshComponent* Comp : MeshComponents)
			{
				if (Comp && Comp->GetStaticMesh())
				{
					bHasValidStaticMeshComponent = true;
					break;
				}
			}
			if (!bHasValidStaticMeshComponent)
			{
				continue;
			}

			// Count only actors that actually have scanable static meshes.
			++ScannedCount;

			bool bHasRisk = false;
			bool bMissingCollisionForActor = false;
			bool bDefaultMaterialForActor = false;
			TArray<FString> ActorRiskTags;
			const FString ActorLabel = Actor->GetActorLabel();

			for (UStaticMeshComponent* SMC : MeshComponents)
			{
				if (!SMC)
				{
					continue;
				}

				UStaticMesh* SM = SMC->GetStaticMesh();
				if (!SM)
				{
					continue;
				}

				if (bCheckMissingCollision && !bMissingCollisionForActor)
				{
					const bool bComponentCollisionDisabled =
						(SMC->GetCollisionEnabled() == ECollisionEnabled::NoCollision) || !SMC->IsCollisionEnabled();

					bool bMeshCollisionInvalid = false;
					UBodySetup* BodySetup = SM->GetBodySetup();
					if (!BodySetup)
					{
						bMeshCollisionInvalid = true;
					}
					else
					{
						const bool bHasPrimitives = BodySetup->AggGeom.GetElementCount() > 0;
						const bool bIsComplexAsSimple =
							BodySetup->CollisionTraceFlag == ECollisionTraceFlag::CTF_UseComplexAsSimple;
						const bool bCollideAll = BodySetup->bMeshCollideAll;
						bMeshCollisionInvalid = !bHasPrimitives && !bIsComplexAsSimple && !bCollideAll;
					}

					if (bComponentCollisionDisabled || bMeshCollisionInvalid)
					{
						bHasRisk = true;
						bMissingCollisionForActor = true;
						ActorRiskTags.Add(TEXT("[MissingCollision]"));
					}
				}

				if (bCheckDefaultMaterial && !bDefaultMaterialForActor)
				{
					bool bHasDefaultMat = false;
					const int32 NumMaterials = SMC->GetNumMaterials();
					if (NumMaterials == 0)
					{
						bHasDefaultMat = true;
					}
					else
					{
						for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
						{
							UMaterialInterface* Mat = SMC->GetMaterial(MaterialIndex);
							if (!Mat ||
								Mat->GetName().Contains(TEXT("Default")) ||
								Mat->GetName().Contains(TEXT("WorldGrid")) ||
								Mat->GetName().Contains(TEXT("BasicShape")) ||
								Mat->GetPathName().StartsWith(TEXT("/Engine/")))
							{
								bHasDefaultMat = true;
								break;
							}
						}
					}

					if (bHasDefaultMat)
					{
						bHasRisk = true;
						bDefaultMaterialForActor = true;
						ActorRiskTags.Add(TEXT("[DefaultMaterial]"));
					}
				}

				if ((!bCheckMissingCollision || bMissingCollisionForActor) &&
					(!bCheckDefaultMaterial || bDefaultMaterialForActor))
				{
					break;
				}
			}

			if (bHasRisk)
			{
				if (bMissingCollisionForActor)
				{
					MissingCollisionActors.Add(ActorLabel);
				}
				if (bDefaultMaterialForActor)
				{
					DefaultMaterialActors.Add(ActorLabel);
				}

				const FString RiskReason =
					ActorRiskTags.Num() > 0 ? FString::Join(ActorRiskTags, TEXT("")) : TEXT("-");
				RiskyActorNames.Add(FString::Printf(TEXT("%s %s"), *ActorLabel, *RiskReason));
				RiskyActorPaths.Add(Actor->GetPathName());
				RiskIssueDetails.Add(RiskReason);
			}
		}

		OutResult = FHCIAgentToolActionResult();
		OutResult.bSucceeded = true;
		OutResult.Reason = TEXT("scan_level_mesh_risks_ok");
		OutResult.EstimatedAffectedCount = RiskyActorNames.Num();

		OutResult.Evidence.Add(TEXT("scope"), Scope);
		OutResult.Evidence.Add(TEXT("requested_actor_count"), FString::FromInt(RequestedActorNames.Num()));
		OutResult.Evidence.Add(TEXT("candidate_actor_count"), FString::FromInt(ActorsToScan.Num()));
		OutResult.Evidence.Add(TEXT("scanned_count"), FString::FromInt(ScannedCount));
		OutResult.Evidence.Add(TEXT("risky_count"), FString::FromInt(RiskyActorNames.Num()));

		FString ReportSummary = FString::Printf(TEXT("Scanned %d actors in world '%s', found %d with risks."), ScannedCount, *World->GetName(), RiskyActorNames.Num());
		OutResult.Evidence.Add(TEXT("risk_summary"), ReportSummary);

		if (RequestedActorNames.Num() > 0)
		{
			OutResult.Evidence.Add(TEXT("actor_names"), FString::Join(RequestedActorNames, TEXT(" | ")));
		}

		FString AffectedActorsStr = RiskyActorNames.Num() > 0 ? FString::Join(RiskyActorNames, TEXT(" | ")) : TEXT("none");
		OutResult.Evidence.Add(TEXT("risky_actors"), AffectedActorsStr);
		OutResult.Evidence.Add(TEXT("actor_path"), RiskyActorPaths.Num() > 0 ? RiskyActorPaths[0] : TEXT("-"));
		OutResult.Evidence.Add(TEXT("issue"), RiskIssueDetails.Num() > 0 ? RiskIssueDetails[0] : TEXT("none"));
		OutResult.Evidence.Add(
			TEXT("evidence"),
			RiskIssueDetails.Num() > 0
				? FString::Printf(TEXT("risk_issues=%s"), *FString::Join(RiskIssueDetails, TEXT(" | ")))
				: TEXT("risk_issues=none"));

		if (MissingCollisionActors.Num() > 0)
		{
			OutResult.Evidence.Add(TEXT("missing_collision_actors"), FString::Join(MissingCollisionActors, TEXT(" | ")));
		}
		else
		{
			OutResult.Evidence.Add(TEXT("missing_collision_actors"), TEXT("none"));
		}

		if (DefaultMaterialActors.Num() > 0)
		{
			OutResult.Evidence.Add(TEXT("default_material_actors"), FString::Join(DefaultMaterialActors, TEXT(" | ")));
		}
		else
		{
			OutResult.Evidence.Add(TEXT("default_material_actors"), TEXT("none"));
		}

		OutResult.Evidence.Add(TEXT("result"), TEXT("scan_level_mesh_risks_ok"));
		return true;
	}
};
}

TSharedPtr<IHCIAgentToolAction> HCIToolActionFactories::MakeScanLevelMeshRisksToolAction()
{
	return MakeShared<FHCIScanLevelMeshRisksToolAction>();
}

