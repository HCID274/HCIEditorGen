#include "Audit/HCIAbilityKitPreviewActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "HCIAbilityKitAsset.h"
#include "UObject/UObjectGlobals.h"

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
#if WITH_EDITOR
	RebindObservedAbilityAsset();
#endif
	ApplyPreviewMesh();
}

void AHCIAbilityKitPreviewActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

void AHCIAbilityKitPreviewActor::RefreshPreview()
{
	ApplyPreviewMesh();
}

#if WITH_EDITOR
void AHCIAbilityKitPreviewActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName ChangedPropertyName = PropertyChangedEvent.GetPropertyName();
	if (ChangedPropertyName == GET_MEMBER_NAME_CHECKED(AHCIAbilityKitPreviewActor, AbilityAsset))
	{
		RebindObservedAbilityAsset();
	}

	ApplyPreviewMesh();
}

void AHCIAbilityKitPreviewActor::RebindObservedAbilityAsset()
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
			&AHCIAbilityKitPreviewActor::HandleObservedObjectPropertyChanged);
	}
}

void AHCIAbilityKitPreviewActor::HandleObservedObjectPropertyChanged(
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
	if (ChangedProperty && ChangedProperty->GetFName() != GET_MEMBER_NAME_CHECKED(UHCIAbilityKitAsset, RepresentingMesh))
	{
		return;
	}

	ApplyPreviewMesh();
}
#endif

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
