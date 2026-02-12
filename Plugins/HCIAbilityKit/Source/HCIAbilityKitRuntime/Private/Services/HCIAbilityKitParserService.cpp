#include "Services/HCIAbilityKitParserService.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
/** 全局 Python 钩子变量，用于存储被注入的解析逻辑喵 */
FHCIAbilityKitPythonHook GPythonHook;
}

void FHCIAbilityKitParserService::SetPythonHook(FHCIAbilityKitPythonHook InHook)
{
	// 使用 MoveTemp 移动函数对象，减少拷贝开销喵
	GPythonHook = MoveTemp(InHook);
}

void FHCIAbilityKitParserService::ClearPythonHook()
{
	GPythonHook = nullptr;
}

bool FHCIAbilityKitParserService::TryParseKitFile(const FString& FullFilename, FHCIAbilityKitParsedData& OutParsed, FString& OutError)
{
	FString FileContent;
	// 首先尝试将文件内容读入字符串中喵
	if (!FFileHelper::LoadFileToString(FileContent, *FullFilename))
	{
		OutError = FString::Printf(TEXT("Failed to load file: %s"), *FullFilename);
		return false;
	}

	// 准备 JSON 解析器喵
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContent);
	// 尝试反序列化 JSON 字符串喵
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Invalid JSON format");
		return false;
	}

	// 解析 schema_version 字段，确保数据格式版本正确喵
	int32 SchemaVersionTmp;
	if (!RootObject->TryGetNumberField(TEXT("schema_version"), SchemaVersionTmp))
	{
		OutError = TEXT("Missing or invalid field: schema_version (must be an integer)");
		return false;
	}
	OutParsed.SchemaVersion = SchemaVersionTmp;

	// 解析技能 ID，它是唯一标识喵
	FString IdTmp;
	if (!RootObject->TryGetStringField(TEXT("id"), IdTmp) || IdTmp.IsEmpty())
	{
		OutError = TEXT("Missing or invalid field: id (must be a non-empty string)");
		return false;
	}
	OutParsed.Id = IdTmp;

	// 解析显示名称喵
	FString DisplayNameTmp;
	if (!RootObject->TryGetStringField(TEXT("display_name"), DisplayNameTmp))
	{
		OutError = TEXT("Missing or invalid field: display_name (must be a string)");
		return false;
	}
	OutParsed.DisplayName = DisplayNameTmp;

	// 解析 params 对象，包含具体的技能数值喵
	const TSharedPtr<FJsonObject>* ParamsObj = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("params"), ParamsObj) || !ParamsObj || !ParamsObj->IsValid())
	{
		OutError = TEXT("Missing or invalid object: params");
		return false;
	}

	// 解析伤害数值喵
	double DamageTmp;
	if (!(*ParamsObj)->TryGetNumberField(TEXT("damage"), DamageTmp))
	{
		OutError = TEXT("Missing or invalid field in params: damage (must be a number)");
		return false;
	}
	OutParsed.Damage = static_cast<float>(DamageTmp);

	// 如果设置了 Python 钩子，则将初步解析的数据传给 Python 进一步处理喵
	if (GPythonHook)
	{
		FString PythonError;
		if (!GPythonHook(FullFilename, OutParsed, PythonError))
		{
			// 如果 Python 逻辑返回失败，则终止导入并记录错误喵
			OutError = PythonError.IsEmpty() ? TEXT("Python processing failed") : PythonError;
			return false;
		}
	}

	return true;
}
