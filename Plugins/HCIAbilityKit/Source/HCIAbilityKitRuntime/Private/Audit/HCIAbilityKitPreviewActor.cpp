#include "Audit/HCIAbilityKitPreviewActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "HCIAbilityKitAsset.h"

AHCIAbilityKitPreviewActor::AHCIAbilityKitPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;

	PreviewMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
	SetRootComponent(PreviewMeshComponent);
	PreviewMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AHCIAbilityKitPreviewActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyPreviewMesh();
}

void AHCIAbilityKitPreviewActor::RefreshPreview()
{
	ApplyPreviewMesh();
}

void AHCIAbilityKitPreviewActor::ApplyPreviewMesh()
{
	if (!PreviewMeshComponent)
	{
		return;
	}

	UStaticMesh* PreviewMesh = nullptr;
	if (AbilityAsset && !AbilityAsset->RepresentingMesh.IsNull())
	{
		PreviewMesh = AbilityAsset->RepresentingMesh.LoadSynchronous();
	}

	PreviewMeshComponent->SetStaticMesh(PreviewMesh);
}
