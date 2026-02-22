#pragma once

#include "CoreMinimal.h"
#include "HCIAbilityKitErrorCodes.h"
#include "Templates/Function.h"

/**
 * FHCIAbilityKitParsedData
 * AbilityKit 源文件解析后的标准化数据结构。
 * 包含了技能的核心元数据，如版本、ID、名称和数值参数。
 */
struct HCIABILITYKITRUNTIME_API FHCIAbilityKitParsedData
{
	/** Schema 版本号，用于数据兼容性校验 */
	int32 SchemaVersion = 1;
	/** 技能的唯一标识符 */
	FString Id;
	/** 技能的显示名称 */
	FString DisplayName;
	/** 技能的基础伤害数值 */
	float Damage = 0.0f;
	/** 代表性网格的 UE 长对象路径（可选） */
	FString RepresentingMeshPath;
	/** JSON 中的 params.triangle_count_lod0 预期值（可选） */
	int32 TriangleCountLod0Expected = INDEX_NONE;
};

/**
 * FHCIAbilityKitParseError
 * 统一错误契约：用于导入/重导入与 Python 钩子的错误回传。
 */
struct HCIABILITYKITRUNTIME_API FHCIAbilityKitParseError
{
	/** 错误码（例如 E1001/E3001） */
	FString Code;
	/** 发生错误的文件路径 */
	FString File;
	/** 错误字段（例如 schema_version/params.damage/python_hook） */
	FString Field;
	/** 人类可读的错误原因 */
	FString Reason;
	/** 针对该错误的修复建议 */
	FString Hint;
	/** 详细错误信息（主要用于日志记录，可选） */
	FString Detail;

	/** 检查错误对象是否包含有效的错误信息 */
	bool IsValid() const;
	/** 将错误信息格式化为统一契约定义的模板字符串 */
	FString ToContractString() const;
};

/** 
 * Python 钩子函数定义
 * 允许在 C++ 完成基础解析后，调用 Python 脚本进行二次处理或数据校验。
 */
using FHCIAbilityKitPythonHook = TFunction<bool(const FString& SourceFilename, FHCIAbilityKitParsedData& InOutParsed, FHCIAbilityKitParseError& OutError)>;

/**
 * FHCIAbilityKitParserService
 * AbilityKit 解析服务：负责读取、解析并校验 .hciabilitykit 源文件。
 * 它是连接外部 JSON 数据与虚幻引擎资产对象的桥梁。
 */
class HCIABILITYKITRUNTIME_API FHCIAbilityKitParserService
{
public:
	/** 设置 Python 钩子，用于注入 Python 端的处理逻辑 */
	static void SetPythonHook(FHCIAbilityKitPythonHook InHook);
	/** 清除已设置的 Python 钩子 */
	static void ClearPythonHook();

	/** 
	 * 尝试解析指定路径的 Kit 文件
	 * @param FullFilename 待解析文件的绝对路径
	 * @param OutParsed 存储解析成功后的结构化数据
	 * @param OutError 如果解析失败，存储详细的结构化错误信息
	 * @return 解析成功返回 true，否则返回 false
	 */
	static bool TryParseKitFile(const FString& FullFilename, FHCIAbilityKitParsedData& OutParsed, FHCIAbilityKitParseError& OutError);

	/**
	 * 兼容性接口：将解析错误直接转换为字符串输出
	 */
	static bool TryParseKitFile(const FString& FullFilename, FHCIAbilityKitParsedData& OutParsed, FString& OutError);
};
