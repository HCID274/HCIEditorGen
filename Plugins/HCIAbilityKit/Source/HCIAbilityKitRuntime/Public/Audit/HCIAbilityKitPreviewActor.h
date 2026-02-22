#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HCIAbilityKitPreviewActor.generated.h"

class UHCIAbilityKitAsset;
class UStaticMeshComponent;

UCLASS(BlueprintType, Blueprintable)
class HCIABILITYKITRUNTIME_API AHCIAbilityKitPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	AHCIAbilityKitPreviewActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(CallInEditor, Category = "HCIAudit")
	void RefreshPreview();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HCIAudit")
	TObjectPtr<UHCIAbilityKitAsset> AbilityAsset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HCIAudit")
	TObjectPtr<UStaticMeshComponent> PreviewMeshComponent;

private:
	void ApplyPreviewMesh();

#if WITH_EDITOR
	void RebindObservedAbilityAsset();
	void HandleObservedObjectPropertyChanged(UObject* ChangedObject, struct FPropertyChangedEvent& PropertyChangedEvent);

	TWeakObjectPtr<UHCIAbilityKitAsset> ObservedAbilityAsset;
	FDelegateHandle ObservedAssetPropertyChangedHandle;
#endif
};
