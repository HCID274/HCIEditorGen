#include "Services/HCIAbilityKitParserService.h"

#include "Dom/JsonObject.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
/** 全局 Python 钩子函数变量，用于存储从模块层注入的自定义校验逻辑 */
FHCIAbilityKitPythonHook GPythonHook;

/** 
 * 构造 FHCIAbilityKitParseError 结构体的辅助函数
 */
FHCIAbilityKitParseError MakeParseError(
	const FString& Code,
	const FString& File,
	const FString& Field,
	const FString& Reason,
	const FString& Hint,
	const FString& Detail = FString())
{
	FHCIAbilityKitParseError ParseError;
	ParseError.Code = Code;
	ParseError.File = File;
	ParseError.Field = Field;
	ParseError.Reason = Reason;
	ParseError.Hint = Hint;
	ParseError.Detail = Detail;
	return ParseError;
}
}

bool FHCIAbilityKitParseError::IsValid() const
{
	// 验证错误对象的核心字段是否已填充
	return !Code.IsEmpty() && !File.IsEmpty() && !Field.IsEmpty() && !Reason.IsEmpty();
}

FString FHCIAbilityKitParseError::ToContractString() const
{
	// 按照项目定义的错误契约格式生成标准错误字符串
	const FString SafeCode = Code.IsEmpty() ? HCIAbilityKitErrorCodes::Unknown : Code;
	const FString SafeFile = File.IsEmpty() ? TEXT("<unknown>") : File;
	const FString SafeField = Field.IsEmpty() ? TEXT("<unknown>") : Field;
	const FString SafeReason = Reason.IsEmpty() ? TEXT("Unknown error") : Reason;
	const FString SafeHint = Hint.IsEmpty() ? TEXT("Check logs for details") : Hint;

	return FString::Printf(
		TEXT("[HCIAbilityKit][Error] code=%s file=%s field=%s reason=%s hint=%s"),
		*SafeCode,
		*SafeFile,
		*SafeField,
		*SafeReason,
		*SafeHint);
}

void FHCIAbilityKitParserService::SetPythonHook(FHCIAbilityKitPythonHook InHook)
{
	// 注入 Python 逻辑，通常在编辑器模块初始化时进行设置
	GPythonHook = MoveTemp(InHook);
}

void FHCIAbilityKitParserService::ClearPythonHook()
{
	// 清除钩子以防内存泄漏或非法调用
	GPythonHook = nullptr;
}

bool FHCIAbilityKitParserService::TryParseKitFile(
	const FString& FullFilename,
	FHCIAbilityKitParsedData& OutParsed,
	FHCIAbilityKitParseError& OutError)
{
	FString FileContent;
	// 1. 读取物理文件内容
	if (!FFileHelper::LoadFileToString(FileContent, *FullFilename))
	{
		OutError = MakeParseError(
			HCIAbilityKitErrorCodes::FileReadError,
			FullFilename,
			TEXT("file"),
			TEXT("Source file is missing or unreadable"),
			TEXT("Verify file path, encoding and read permission"));
		return false;
	}

	// 2. 将字符串反序列化为 JSON 对象
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = MakeParseError(
			HCIAbilityKitErrorCodes::InvalidJson,
			FullFilename,
			TEXT("root"),
			TEXT("Invalid JSON format"),
			TEXT("Validate JSON syntax and remove trailing commas"));
		return false;
	}

	// 3. 校验并提取 schema_version
	if (!RootObject->HasField(TEXT("schema_version")))
	{
		OutError = MakeParseError(
			HCIAbilityKitErrorCodes::MissingField,
			FullFilename,
			TEXT("schema_version"),
			TEXT("Missing required field"),
			TEXT("Add integer field schema_version"));
		return false;
	}

	int32 SchemaVersionTmp;
	if (!RootObject->TryGetNumberField(TEXT("schema_version"), SchemaVersionTmp))
	{
		OutError = MakeParseError(
			HCIAbilityKitErrorCodes::InvalidFieldType,
			FullFilename,
			TEXT("schema_version"),
			TEXT("Invalid field type"),
			TEXT("schema_version must be an integer"));
		return false;
	}
	if (SchemaVersionTmp != 1)
	{
		OutError = MakeParseError(
			HCIAbilityKitErrorCodes::InvalidFieldValue,
			FullFilename,
			TEXT("schema_version"),
			FString::Printf(TEXT("Unsupported schema_version: %d"), SchemaVersionTmp),
			TEXT("Use schema_version = 1"));
		return false;
	}
	OutParsed.SchemaVersion = SchemaVersionTmp;

	// 4. 解析技能标识符 (id)
	if (!RootObject->HasField(TEXT("id")))
	{
		OutError = MakeParseError(
			HCIAbilityKitErrorCodes::MissingField,
			FullFilename,
			TEXT("id"),
			TEXT("Missing required field"),
			TEXT("Add non-empty string field id"));
		return false;
	}

	FString IdTmp;
	if (!RootObject->TryGetStringField(TEXT("id"), IdTmp))
	{
		OutError = MakeParseError(
			HCIAbilityKitErrorCodes::InvalidFieldType,
			FullFilename,
			TEXT("id"),
			TEXT("Invalid field type"),
			TEXT("id must be a string"));
		return false;
	}
	if (IdTmp.IsEmpty())
	{
		OutError = MakeParseError(
			HCIAbilityKitErrorCodes::InvalidFieldValue,
			FullFilename,
			TEXT("id"),
			TEXT("Invalid field value"),
			TEXT("id must be a non-empty string"));
		return false;
	}
	OutParsed.Id = IdTmp;

	// 5. 解析显示名称 (display_name)
	if (!RootObject->HasField(TEXT("display_name")))
	{
		OutError = MakeParseError(
			HCIAbilityKitErrorCodes::MissingField,
			FullFilename,
			TEXT("display_name"),
			TEXT("Missing required field"),
			TEXT("Add string field display_name"));
		return false;
	}

	FString DisplayNameTmp;
	if (!RootObject->TryGetStringField(TEXT("display_name"), DisplayNameTmp))
	{
		OutError = MakeParseError(
			HCIAbilityKitErrorCodes::InvalidFieldType,
			FullFilename,
			TEXT("display_name"),
			TEXT("Invalid field type"),
			TEXT("display_name must be a string"));
		return false;
	}
	OutParsed.DisplayName = DisplayNameTmp;

	// 6. 解析代表性网格路径 (representing_mesh，可选)
	if (RootObject->HasField(TEXT("representing_mesh")))
	{
		FString RepresentingMeshPathTmp;
		if (!RootObject->TryGetStringField(TEXT("representing_mesh"), RepresentingMeshPathTmp))
		{
			OutError = MakeParseError(
				HCIAbilityKitErrorCodes::InvalidFieldType,
				FullFilename,
				TEXT("representing_mesh"),
				TEXT("Invalid field type"),
				TEXT("representing_mesh must be a string"));
			return false;
		}
		OutParsed.RepresentingMeshPath = RepresentingMeshPathTmp;
	}
	else
	{
		OutParsed.RepresentingMeshPath.Reset();
	}

	// 7. 解析参数对象 (params)
	if (!RootObject->HasField(TEXT("params")))
	{
		OutError = MakeParseError(
			HCIAbilityKitErrorCodes::MissingField,
			FullFilename,
			TEXT("params"),
			TEXT("Missing required field"),
			TEXT("Add object field params"));
		return false;
	}

	const TSharedPtr<FJsonObject>* ParamsObj = nullptr;
	if (!RootObject->TryGetObjectField(TEXT("params"), ParamsObj) || !ParamsObj || !ParamsObj->IsValid())
	{
		OutError = MakeParseError(
			HCIAbilityKitErrorCodes::InvalidFieldType,
			FullFilename,
			TEXT("params"),
			TEXT("Invalid field type"),
			TEXT("params must be an object"));
		return false;
	}
	if (!(*ParamsObj)->HasField(TEXT("damage")))
	{
		OutError = MakeParseError(
			HCIAbilityKitErrorCodes::MissingField,
			FullFilename,
			TEXT("params.damage"),
			TEXT("Missing required field"),
			TEXT("Add numeric field params.damage"));
		return false;
	}

	// 8. 解析基础伤害数值 (damage)
	double DamageTmp;
	if (!(*ParamsObj)->TryGetNumberField(TEXT("damage"), DamageTmp))
	{
		OutError = MakeParseError(
			HCIAbilityKitErrorCodes::InvalidFieldType,
			FullFilename,
			TEXT("params.damage"),
			TEXT("Invalid field type"),
			TEXT("params.damage must be a number"));
		return false;
	}
	OutParsed.Damage = static_cast<float>(DamageTmp);

	// 8.1 解析可选三角面预期值 (params.triangle_count_lod0)
	if ((*ParamsObj)->HasField(TEXT("triangle_count_lod0")))
	{
		double TriangleExpectedTmp = 0.0;
		if (!(*ParamsObj)->TryGetNumberField(TEXT("triangle_count_lod0"), TriangleExpectedTmp))
		{
			OutError = MakeParseError(
				HCIAbilityKitErrorCodes::InvalidFieldType,
				FullFilename,
				TEXT("params.triangle_count_lod0"),
				TEXT("Invalid field type"),
				TEXT("params.triangle_count_lod0 must be a number"));
			return false;
		}

		if (TriangleExpectedTmp < 0.0 || TriangleExpectedTmp > static_cast<double>(MAX_int32))
		{
			OutError = MakeParseError(
				HCIAbilityKitErrorCodes::InvalidFieldValue,
				FullFilename,
				TEXT("params.triangle_count_lod0"),
				TEXT("Invalid field value"),
				TEXT("params.triangle_count_lod0 must be >= 0 and <= 2147483647"));
			return false;
		}

		OutParsed.TriangleCountLod0Expected = FMath::RoundToInt(TriangleExpectedTmp);
	}

	// 9. 调用 Python 钩子脚本进行深度处理或校验
	if (GPythonHook)
	{
		if (!GPythonHook(FullFilename, OutParsed, OutError))
		{
			// 如果 Python 返回失败且未填充结构化错误，则补充默认错误信息
			if (!OutError.IsValid())
			{
				OutError = MakeParseError(
					HCIAbilityKitErrorCodes::PythonError,
					FullFilename,
					TEXT("python_hook"),
					TEXT("Python processing failed"),
					TEXT("Check Python hook output and editor log"));
			}
			return false;
		}
	}

	return true;
}

bool FHCIAbilityKitParserService::TryParseKitFile(
	const FString& FullFilename,
	FHCIAbilityKitParsedData& OutParsed,
	FString& OutError)
{
	// 封装接口，将结构化错误转换为字符串返回给调用方
	FHCIAbilityKitParseError ParseError;
	const bool bOk = TryParseKitFile(FullFilename, OutParsed, ParseError);
	if (!bOk)
	{
		OutError = ParseError.ToContractString();
		if (!ParseError.Detail.IsEmpty())
		{
			OutError += FString::Printf(TEXT(" detail=%s"), *ParseError.Detail);
		}
	}
	return bOk;
}
