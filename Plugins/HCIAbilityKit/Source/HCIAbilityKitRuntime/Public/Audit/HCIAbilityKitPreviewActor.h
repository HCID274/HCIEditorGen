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

	UFUNCTION(CallInEditor, Category = "HCIAudit")
	void RefreshPreview();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HCIAudit")
	TObjectPtr<UHCIAbilityKitAsset> AbilityAsset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HCIAudit")
	TObjectPtr<UStaticMeshComponent> PreviewMeshComponent;

private:
	void ApplyPreviewMesh();
};
