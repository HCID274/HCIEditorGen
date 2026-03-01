#pragma once

#include "CoreMinimal.h"

enum class EHCIToolCapability : uint8
{
	ReadOnly,
	Write,
	Destructive
};

enum class EHCIToolArgValueType : uint8
{
	String,
	StringArray,
	Int
};

struct HCIRUNTIME_API FHCIToolArgSchema
{
	FName ArgName;
	EHCIToolArgValueType ValueType = EHCIToolArgValueType::String;
	bool bRequired = true;

	int32 MinArrayLength = INDEX_NONE;
	int32 MaxArrayLength = INDEX_NONE;

	int32 MinStringLength = INDEX_NONE;
	int32 MaxStringLength = INDEX_NONE;
	FString RegexPattern;

	int32 MinIntValue = INDEX_NONE;
	int32 MaxIntValue = INDEX_NONE;

	TArray<FString> AllowedStringValues;
	TArray<int32> AllowedIntValues;

	bool bMustStartWithGamePath = false;
	bool bStringArrayAllowsSubsetOfEnum = false;
	bool bRequiresPipelineInput = false;
};

struct HCIRUNTIME_API FHCIToolDescriptor
{
	FName ToolName;
	TArray<FHCIToolArgSchema> ArgsSchema;
	EHCIToolCapability Capability = EHCIToolCapability::ReadOnly;
	bool bSupportsDryRun = false;
	bool bSupportsUndo = false;
	bool bDestructive = false;
	FString Domain;
	FString Summary;
};

class HCIRUNTIME_API FHCIToolRegistry
{
public:
	static FHCIToolRegistry& Get();
	static const FHCIToolRegistry& GetReadOnly();

	void ResetToDefaults();
	bool RegisterTool(const FHCIToolDescriptor& InDescriptor, FString* OutError = nullptr);

	const FHCIToolDescriptor* FindTool(FName ToolName) const;
	bool IsWhitelistedTool(FName ToolName) const;
	TArray<FHCIToolDescriptor> GetAllTools() const;
	TArray<FName> GetRegisteredToolNames() const;

	bool ValidateFrozenDefaults(FString& OutError) const;

	static FString CapabilityToString(EHCIToolCapability Capability);
	static FString ArgValueTypeToString(EHCIToolArgValueType ValueType);

private:
	FHCIToolRegistry() = default;
	~FHCIToolRegistry() = default;
	FHCIToolRegistry(const FHCIToolRegistry&) = delete;
	FHCIToolRegistry& operator=(const FHCIToolRegistry&) = delete;
	FHCIToolRegistry(FHCIToolRegistry&&) = delete;
	FHCIToolRegistry& operator=(FHCIToolRegistry&&) = delete;

	void EnsureDefaultTools() const;

	mutable bool bDefaultsInitialized = false;
	mutable TArray<FHCIToolDescriptor> Tools;
	mutable TMap<FName, int32> ToolIndexByName;
};


