#pragma once

#include "CoreMinimal.h"

enum class EHCIAbilityKitToolCapability : uint8
{
	ReadOnly,
	Write,
	Destructive
};

enum class EHCIAbilityKitToolArgValueType : uint8
{
	String,
	StringArray,
	Int
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitToolArgSchema
{
	FName ArgName;
	EHCIAbilityKitToolArgValueType ValueType = EHCIAbilityKitToolArgValueType::String;
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
};

struct HCIABILITYKITRUNTIME_API FHCIAbilityKitToolDescriptor
{
	FName ToolName;
	TArray<FHCIAbilityKitToolArgSchema> ArgsSchema;
	EHCIAbilityKitToolCapability Capability = EHCIAbilityKitToolCapability::ReadOnly;
	bool bSupportsDryRun = false;
	bool bSupportsUndo = false;
	bool bDestructive = false;
	FString Domain;
	FString Summary;
};

class HCIABILITYKITRUNTIME_API FHCIAbilityKitToolRegistry
{
public:
	static FHCIAbilityKitToolRegistry& Get();

	void ResetToDefaults();
	bool RegisterTool(const FHCIAbilityKitToolDescriptor& InDescriptor, FString* OutError = nullptr);

	const FHCIAbilityKitToolDescriptor* FindTool(FName ToolName) const;
	bool IsWhitelistedTool(FName ToolName) const;
	TArray<FHCIAbilityKitToolDescriptor> GetAllTools() const;
	TArray<FName> GetRegisteredToolNames() const;

	bool ValidateFrozenDefaults(FString& OutError) const;

	static FString CapabilityToString(EHCIAbilityKitToolCapability Capability);
	static FString ArgValueTypeToString(EHCIAbilityKitToolArgValueType ValueType);

private:
	FHCIAbilityKitToolRegistry() = default;
	~FHCIAbilityKitToolRegistry() = default;
	FHCIAbilityKitToolRegistry(const FHCIAbilityKitToolRegistry&) = delete;
	FHCIAbilityKitToolRegistry& operator=(const FHCIAbilityKitToolRegistry&) = delete;
	FHCIAbilityKitToolRegistry(FHCIAbilityKitToolRegistry&&) = delete;
	FHCIAbilityKitToolRegistry& operator=(FHCIAbilityKitToolRegistry&&) = delete;

	void EnsureDefaultTools() const;

	mutable bool bDefaultsInitialized = false;
	mutable TArray<FHCIAbilityKitToolDescriptor> Tools;
	mutable TMap<FName, int32> ToolIndexByName;
};

