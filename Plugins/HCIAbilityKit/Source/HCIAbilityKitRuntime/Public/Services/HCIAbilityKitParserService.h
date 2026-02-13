#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

/**
 * FHCIAbilityKitParsedData
 * AbilityKit 源文件解析后的标准化数据结构喵。
 * 包含了技能的核心元数据，如版本、ID、名称和数值参数喵。
 */
struct HCIABILITYKITRUNTIME_API FHCIAbilityKitParsedData
{
	/** Schema 版本号，用于数据兼容性校验喵 */
	int32 SchemaVersion = 1;
	/** 技能的唯一标识符喵 */
	FString Id;
	/** 技能的显示名称喵 */
	FString DisplayName;
	/** 技能的基础伤害数值喵 */
	float Damage = 0.0f;
};

/**
 * FHCIAbilityKitParseError
 * 统一错误契约：用于 Import/Reimport 与 Python Hook 的错误回传喵。
 */
struct HCIABILITYKITRUNTIME_API FHCIAbilityKitParseError
{
	/** 错误码（例如 E1001/E3001）喵 */
	FString Code;
	/** 发生错误的文件路径喵 */
	FString File;
	/** 错误字段（例如 schema_version/params.damage/python_hook）喵 */
	FString Field;
	/** 人类可读原因喵 */
	FString Reason;
	/** 修复建议喵 */
	FString Hint;
	/** 详细信息（日志用，可选）喵 */
	FString Detail;

	/** 是否包含有效错误信息喵 */
	bool IsValid() const;
	/** 输出统一错误模板字符串喵 */
	FString ToContractString() const;
};

/** 
 * Python 钩子函数定义喵 
 * 允许在 C++ 解析后调用 Python 脚本进行二次处理或校验喵。
 */
using FHCIAbilityKitPythonHook = TFunction<bool(const FString& SourceFilename, FHCIAbilityKitParsedData& InOutParsed, FHCIAbilityKitParseError& OutError)>;

/**
 * FHCIAbilityKitParserService
 * AbilityKit 解析服务：负责读取与校验 .hciabilitykit 源文件喵。
 * 它是连接外部 JSON 数据与 UE 资产对象的桥梁喵。
 */
class HCIABILITYKITRUNTIME_API FHCIAbilityKitParserService
{
public:
	/** 设置 Python 钩子，用于注入 Python 端的处理逻辑喵 */
	static void SetPythonHook(FHCIAbilityKitPythonHook InHook);
	/** 清除 Python 钩子喵 */
	static void ClearPythonHook();

	/** 
	 * 尝试解析指定路径的 Kit 文件喵 
	 * @param FullFilename 文件的绝对路径喵
	 * @param OutParsed 解析成功后的数据承载体喵
	 * @param OutError 如果解析失败，存储结构化错误信息喵
	 * @return 解析成功返回 true，否则返回 false 喵
	 */
	static bool TryParseKitFile(const FString& FullFilename, FHCIAbilityKitParsedData& OutParsed, FHCIAbilityKitParseError& OutError);

	/**
	 * 兼容接口：将结构化错误转为字符串输出喵
	 */
	static bool TryParseKitFile(const FString& FullFilename, FHCIAbilityKitParsedData& OutParsed, FString& OutError);
};
