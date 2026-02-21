#pragma once

#include "CoreMinimal.h"

/**
 * HCIAbilityKitErrorCodes
 * Centralized error codes for HCIAbilityKit.
 * These codes follow the standard contract defined in the project.
 */
namespace HCIAbilityKitErrorCodes
{
	/** Default/Unknown error code */
	static const TCHAR* const Unknown = TEXT("E0000");

	/** Invalid JSON format */
	static const TCHAR* const InvalidJson = TEXT("E1001");

	/** Missing required field in JSON */
	static const TCHAR* const MissingField = TEXT("E1002");

	/** Field has an incorrect data type */
	static const TCHAR* const InvalidFieldType = TEXT("E1003");

	/** Field has an invalid value or unsupported version */
	static const TCHAR* const InvalidFieldValue = TEXT("E1004");

	/** Source file is missing or unreadable */
	static const TCHAR* const FileReadError = TEXT("E1005");

	/** Invalid Unreal Engine object path or asset error */
	static const TCHAR* const InvalidObjectPath = TEXT("E1006");

	/** Error during Python hook processing */
	static const TCHAR* const PythonError = TEXT("E3001");

	/** Python hook script not found */
	static const TCHAR* const PythonScriptMissing = TEXT("E3002");

	/** PythonScriptPlugin is unavailable */
	static const TCHAR* const PythonPluginDisabled = TEXT("E3003");
}
