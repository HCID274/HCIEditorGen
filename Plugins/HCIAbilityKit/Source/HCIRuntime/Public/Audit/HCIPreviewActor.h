#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HCIPreviewActor.generated.h"

class UHCIAsset;
class UStaticMeshComponent;

UCLASS(BlueprintType, Blueprintable)
class HCIRUNTIME_API AHCIPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	AHCIPreviewActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(CallInEditor, Category = "HCIAudit")
	void RefreshPreview();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HCIAudit")
	TObjectPtr<UHCIAsset> AbilityAsset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HCIAudit")
	TObjectPtr<UStaticMeshComponent> PreviewMeshComponent;

private:
	void ApplyPreviewMesh();

#if WITH_EDITOR
	void RebindObservedAbilityAsset();
	void HandleObservedObjectPropertyChanged(UObject* ChangedObject, struct FPropertyChangedEvent& PropertyChangedEvent);

	TWeakObjectPtr<UHCIAsset> ObservedAbilityAsset;
	FDelegateHandle ObservedAssetPropertyChangedHandle;
#endif
};


