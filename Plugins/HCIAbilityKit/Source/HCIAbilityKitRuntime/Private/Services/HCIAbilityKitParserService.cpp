#include "Services/HCIAbilityKitParserService.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
/** 全局 Python 钩子变量，用于存储被注入的解析逻辑喵 */
FHCIAbilityKitPythonHook GPythonHook;

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
	return !Code.IsEmpty() && !File.IsEmpty() && !Field.IsEmpty() && !Reason.IsEmpty();
}

FString FHCIAbilityKitParseError::ToContractString() const
{
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
	// 使用 MoveTemp 移动函数对象，减少拷贝开销喵
	GPythonHook = MoveTemp(InHook);
}

void FHCIAbilityKitParserService::ClearPythonHook()
{
	GPythonHook = nullptr;
}

bool FHCIAbilityKitParserService::TryParseKitFile(
	const FString& FullFilename,
	FHCIAbilityKitParsedData& OutParsed,
	FHCIAbilityKitParseError& OutError)
{
	FString FileContent;
	// 首先尝试将文件内容读入字符串中喵
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

	// 准备 JSON 解析器喵
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContent);
	// 尝试反序列化 JSON 字符串喵
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

	// 解析 schema_version 字段，确保数据格式版本正确喵
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

	// 解析技能 ID，它是唯一标识喵
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

	// 解析显示名称喵
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

	// 解析 params 对象，包含具体的技能数值喵
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

	// 解析伤害数值喵
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

	// 如果设置了 Python 钩子，则将初步解析的数据传给 Python 进一步处理喵
	if (GPythonHook)
	{
		if (!GPythonHook(FullFilename, OutParsed, OutError))
		{
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
