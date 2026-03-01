#include "Audit/HCIPreviewActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "HCIAsset.h"
#include "UObject/UObjectGlobals.h"

AHCIPreviewActor::AHCIPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;

	PreviewMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
	SetRootComponent(PreviewMeshComponent);
	PreviewMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AHCIPreviewActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
#if WITH_EDITOR
	RebindObservedAbilityAsset();
#endif
	ApplyPreviewMesh();
}

void AHCIPreviewActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
#if WITH_EDITOR
	if (ObservedAssetPropertyChangedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(ObservedAssetPropertyChangedHandle);
		ObservedAssetPropertyChangedHandle.Reset();
	}
#endif
	Super::EndPlay(EndPlayReason);
}

void AHCIPreviewActor::RefreshPreview()
{
	ApplyPreviewMesh();
}

#if WITH_EDITOR
void AHCIPreviewActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName ChangedPropertyName = PropertyChangedEvent.GetPropertyName();
	if (ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AHCIPreviewActor, AbilityAsset))
	{
		RebindObservedAbilityAsset();
	}

	ApplyPreviewMesh();
}

void AHCIPreviewActor::RebindObservedAbilityAsset()
{
	if (ObservedAssetPropertyChangedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(ObservedAssetPropertyChangedHandle);
		ObservedAssetPropertyChangedHandle.Reset();
	}

	ObservedAbilityAsset = AbilityAsset;
	if (ObservedAbilityAsset.IsValid())
	{
		ObservedAssetPropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(
			this,
			&AHCIPreviewActor::HandleObservedObjectPropertyChanged);
	}
}

void AHCIPreviewActor::HandleObservedObjectPropertyChanged(
	UObject* ChangedObject,
	FPropertyChangedEvent& PropertyChangedEvent)
{
	if (ChangedObject != ObservedAbilityAsset.Get())
	{
		return;
	}

	const FProperty* ChangedProperty = PropertyChangedEvent.Property
		? PropertyChangedEvent.Property
		: PropertyChangedEvent.MemberProperty;
	if (ChangedProperty && ChangedProperty->GetFName() != GET_MEMBER_NAME_CHECKED(UHCIAsset, RepresentingMesh))
	{
		return;
	}

	ApplyPreviewMesh();
}
#endif

void AHCIPreviewActor::ApplyPreviewMesh()
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

