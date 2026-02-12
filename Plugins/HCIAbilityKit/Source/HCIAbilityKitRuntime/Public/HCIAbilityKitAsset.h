#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HCIAbilityKitAsset.generated.h"

#if WITH_EDITORONLY_DATA
class UAssetImportData;
class FAssetRegistryTagsContext;
#endif

/**
 * HCI 技能组件资产类
 * 用于存储从外部数据源导入的技能定义数据喵。
 */
UCLASS(BlueprintType)
class HCIABILITYKITRUNTIME_API UHCIAbilityKitAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Schema 版本号，用于后续数据迁移和兼容性处理喵 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HCI|Schema")
	int32 SchemaVersion = 1;

	/** 技能的唯一标识符喵 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HCI|Identity")
	FString Id;

	/** 技能的显示名称喵 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HCI|Identity")
	FString DisplayName;

	/** 技能造成的基础伤害数值喵 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HCI|Params")
	float Damage = 0.0f;

#if WITH_EDITORONLY_DATA
	/** 资产导入数据，记录了源文件的路径、哈希值等信息，用于实现 Reimport 功能喵 */
	UPROPERTY(VisibleAnywhere, Instanced, Category = "HCI|Import")
	TObjectPtr<UAssetImportData> AssetImportData;

	virtual void PostInitProperties() override;
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
#endif
};
