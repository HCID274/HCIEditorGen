// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HCIAbilityKitAsset.generated.h"

#if WITH_EDITORONLY_DATA
class UAssetImportData;
class FAssetRegistryTagsContext;
#endif

/**
 * 自定义数据资产，接下来做导入/重导入的核心数据载体
 */
UCLASS(BlueprintType)
class HCIEDITORGEN_API UHCIAbilityKitAsset : public UDataAsset //继承自UDataAsset,可以在引擎的内容浏览器右键创建一个这种类型的资产，像创建纹理和材质一样
{
	GENERATED_BODY()
	
public://游戏运行时用的
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HCI|Schema")
	int32 SchemaVersion = 1;//版本号

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HCI|Identity")
	FString Id;//技能的唯一标识符

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HCI|Identity")
	FString DisplayName;//显示给玩家看的名字
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HCI|Params")
	float Damage = 0.0f;//具体的业务数值（伤害值）

#if WITH_EDITORONLY_DATA //预处理指令，告诉编译器，中间包裹的这段代码，只在编辑器模式下编译，打包发布游戏时将其删掉
	//因为玩家不需要知道这个源文件地址，去掉能缩小体积节省内存
	UPROPERTY(VisibleAnywhere, Instanced, Category="HCI|Import")
	TObjectPtr<UAssetImportData> AssetImportData;

	//为了实现Reimport重导入功能，当修改表格，引擎点击Reimport，工具链就知道去哪找原来的文件
	virtual void PostInitProperties() override;
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
#endif	
};
