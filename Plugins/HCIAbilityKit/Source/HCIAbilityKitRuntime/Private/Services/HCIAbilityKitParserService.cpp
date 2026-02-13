#include "Services/HCIAbilityKitParserService.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
/** 全局 Python 钩子函数，用于存储从模块层注入的 Python 校验逻辑喵 */
FHCIAbilityKitPythonHook GPythonHook;

/** 
 * 快速构建结构化错误对象的辅助工具喵 
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
	// 判断错误对象是否被正确初始化，Code/File/Field/Reason 是最基本的四要素喵
	return !Code.IsEmpty() && !File.IsEmpty() && !Field.IsEmpty() && !Reason.IsEmpty();
}

FString FHCIAbilityKitParseError::ToContractString() const
{
	// 按照“统一错误契约”格式化字符串，方便日志监控或外部工具解析喵
	const FString SafeCode = Code.IsEmpty() ? TEXT("E0000") : Code;
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
	// 注入 Python 处理逻辑，通常在 Editor 模块启动时调用喵
	GPythonHook = MoveTemp(InHook);
}

void FHCIAbilityKitParserService::ClearPythonHook()
{
	// 清理钩子，防止模块卸载后发生野指针调用喵
	GPythonHook = nullptr;
}

bool FHCIAbilityKitParserService::TryParseKitFile(
	const FString& FullFilename,
	FHCIAbilityKitParsedData& OutParsed,
	FHCIAbilityKitParseError& OutError)
{
	FString FileContent;
	// 1. 加载物理文件喵
	if (!FFileHelper::LoadFileToString(FileContent, *FullFilename))
	{
		OutError = MakeParseError(
			TEXT("E1005"),
			FullFilename,
			TEXT("file"),
			TEXT("Source file is missing or unreadable"),
			TEXT("Verify file path, encoding and read permission"));
		return false;
	}

	// 2. 解析 JSON 结构喵
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContent);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = MakeParseError(
			TEXT("E1001"),
			FullFilename,
			TEXT("root"),
			TEXT("Invalid JSON format"),
			TEXT("Validate JSON syntax and remove trailing commas"));
		return false;
	}

	// 3. 校验 Schema 版本，确保兼容性喵
	if (!RootObject->HasField(TEXT("schema_version")))
	{
		OutError = MakeParseError(
			TEXT("E1002"),
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
			TEXT("E1003"),
			FullFilename,
			TEXT("schema_version"),
			TEXT("Invalid field type"),
			TEXT("schema_version must be an integer"));
		return false;
	}
	if (SchemaVersionTmp != 1)
	{
		OutError = MakeParseError(
			TEXT("E1004"),
			FullFilename,
			TEXT("schema_version"),
			FString::Printf(TEXT("Unsupported schema_version: %d"), SchemaVersionTmp),
			TEXT("Use schema_version = 1"));
		return false;
	}
	OutParsed.SchemaVersion = SchemaVersionTmp;

	// 4. 解析 ID 字段喵
	if (!RootObject->HasField(TEXT("id")))
	{
		OutError = MakeParseError(
			TEXT("E1002"),
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
			TEXT("E1003"),
			FullFilename,
			TEXT("id"),
			TEXT("Invalid field type"),
			TEXT("id must be a string"));
		return false;
	}
	if (IdTmp.IsEmpty())
	{
		OutError = MakeParseError(
			TEXT("E1004"),
			FullFilename,
			TEXT("id"),
			TEXT("Invalid field value"),
			TEXT("id must be a non-empty string"));
		return false;
	}
	OutParsed.Id = IdTmp;

	// 5. 解析显示名称喵
	if (!RootObject->HasField(TEXT("display_name")))
	{
		OutError = MakeParseError(
			TEXT("E1002"),
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
			TEXT("E1003"),
			FullFilename,
			TEXT("display_name"),
			TEXT("Invalid field type"),
			TEXT("display_name must be a string"));
		return false;
	}
	OutParsed.DisplayName = DisplayNameTmp;

	// 6. 解析 params 参数对象喵
	if (!RootObject->HasField(TEXT("params")))
	{
		OutError = MakeParseError(
			TEXT("E1002"),
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
			TEXT("E1003"),
			FullFilename,
			TEXT("params"),
			TEXT("Invalid field type"),
			TEXT("params must be an object"));
		return false;
	}
	if (!(*ParamsObj)->HasField(TEXT("damage")))
	{
		OutError = MakeParseError(
			TEXT("E1002"),
			FullFilename,
			TEXT("params.damage"),
			TEXT("Missing required field"),
			TEXT("Add numeric field params.damage"));
		return false;
	}

	// 7. 解析具体的伤害数值喵
	double DamageTmp;
	if (!(*ParamsObj)->TryGetNumberField(TEXT("damage"), DamageTmp))
	{
		OutError = MakeParseError(
			TEXT("E1003"),
			FullFilename,
			TEXT("params.damage"),
			TEXT("Invalid field type"),
			TEXT("params.damage must be a number"));
		return false;
	}
	OutParsed.Damage = static_cast<float>(DamageTmp);

	// 8. 执行 Python 钩子（如果已注入）进行二次验证喵
	if (GPythonHook)
	{
		if (!GPythonHook(FullFilename, OutParsed, OutError))
		{
			// 如果 Python 逻辑返回 false 且没有填充错误对象，填充一个默认的 Python 失败错误喵
			if (!OutError.IsValid())
			{
				OutError = MakeParseError(
					TEXT("E3001"),
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
	// 这是一个简单的封装接口，将结构化错误转换为字符串返回喵
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
