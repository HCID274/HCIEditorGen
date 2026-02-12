#pragma once

#include "CoreMinimal.h"
#include "EditorReimportHandler.h"
#include "Factories/Factory.h"
#include "HCIAbilityKitFactory.generated.h"

/**
 * HCI 技能组件工厂类
 * 负责处理 .hciabilitykit 文件的导入和重导入逻辑喵。
 */
UCLASS()
class HCIABILITYKITEDITOR_API UHCIAbilityKitFactory : public UFactory, public FReimportHandler
{
	GENERATED_BODY()

public:
	UHCIAbilityKitFactory();

	/** 检查文件是否可以被此工厂导入喵 */
	virtual bool FactoryCanImport(const FString& Filename) override;

	/** 执行具体的文件导入逻辑，创建新的资产对象喵 */
	virtual UObject* FactoryCreateFile(
		UClass* InClass,
		UObject* InParent,
		FName InName,
		EObjectFlags Flags,
		const FString& Filename,
		const TCHAR* Parms,
		FFeedbackContext* Warn,
		bool& bOutOperationCanceled) override;

	/** 检查指定对象是否支持重导入，并返回源文件路径喵 */
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	/** 设置资产的重导入路径喵 */
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	/** 执行资产的重导入逻辑喵 */
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
	/** 获取重导入处理器的优先级，数值越大优先级越高喵 */
	virtual int32 GetPriority() const override { return 0; }

private:
	/** 用于暂存解析后的技能数据结构喵 */
	struct FParsedKit
	{
		int32 SchemaVersion = 1;
		FString Id;
		FString DisplayName;
		float Damage = 0.0f;
	};

	/** 解析 .hciabilitykit JSON 文件的辅助函数喵 */
	static bool TryParseKitFile(const FString& FullFilename, FParsedKit& OutParsed, FString& OutError);
	/** 将解析后的数据应用到资产对象上喵 */
	static void ApplyParsedToAsset(class UHCIAbilityKitAsset* Asset, const FParsedKit& Parsed);
};
