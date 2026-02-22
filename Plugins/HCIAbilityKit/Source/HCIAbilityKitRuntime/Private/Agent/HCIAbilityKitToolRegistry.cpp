#include "Agent/HCIAbilityKitToolRegistry.h"

namespace
{
static FHCIAbilityKitToolArgSchema MakeStringArrayArg(const TCHAR* ArgName, const int32 MinLen, const int32 MaxLen)
{
	FHCIAbilityKitToolArgSchema Arg;
	Arg.ArgName = FName(ArgName);
	Arg.ValueType = EHCIAbilityKitToolArgValueType::StringArray;
	Arg.MinArrayLength = MinLen;
	Arg.MaxArrayLength = MaxLen;
	return Arg;
}

static FHCIAbilityKitToolArgSchema MakeStringArg(const TCHAR* ArgName)
{
	FHCIAbilityKitToolArgSchema Arg;
	Arg.ArgName = FName(ArgName);
	Arg.ValueType = EHCIAbilityKitToolArgValueType::String;
	return Arg;
}

static FHCIAbilityKitToolArgSchema MakeIntArg(const TCHAR* ArgName)
{
	FHCIAbilityKitToolArgSchema Arg;
	Arg.ArgName = FName(ArgName);
	Arg.ValueType = EHCIAbilityKitToolArgValueType::Int;
	return Arg;
}
}

FHCIAbilityKitToolRegistry& FHCIAbilityKitToolRegistry::Get()
{
	static FHCIAbilityKitToolRegistry Instance;
	return Instance;
}

void FHCIAbilityKitToolRegistry::ResetToDefaults()
{
	Tools.Reset();
	ToolIndexByName.Reset();
	bDefaultsInitialized = true;

	auto RegisterDefault = [this](FHCIAbilityKitToolDescriptor&& Descriptor)
	{
		FString Error;
		RegisterTool(Descriptor, &Error);
	};

	{
		FHCIAbilityKitToolDescriptor Tool;
		Tool.ToolName = TEXT("ScanAssets");
		Tool.Capability = EHCIAbilityKitToolCapability::ReadOnly;
		Tool.bSupportsDryRun = false;
		Tool.bSupportsUndo = false;
		Tool.bDestructive = false;
		Tool.Domain = TEXT("AssetCompliance");
		Tool.Summary = TEXT("Read-only asset audit scan entry point.");
		RegisterDefault(MoveTemp(Tool));
	}

	{
		FHCIAbilityKitToolDescriptor Tool;
		Tool.ToolName = TEXT("SetTextureMaxSize");
		Tool.Capability = EHCIAbilityKitToolCapability::Write;
		Tool.bSupportsDryRun = true;
		Tool.bSupportsUndo = true;
		Tool.bDestructive = false;
		Tool.Domain = TEXT("AssetCompliance");
		Tool.Summary = TEXT("Set texture maximum size using frozen allowed enum values.");

		Tool.ArgsSchema.Add(MakeStringArrayArg(TEXT("asset_paths"), 1, 50));

		FHCIAbilityKitToolArgSchema MaxSizeArg = MakeIntArg(TEXT("max_size"));
		MaxSizeArg.AllowedIntValues = {256, 512, 1024, 2048, 4096, 8192};
		Tool.ArgsSchema.Add(MoveTemp(MaxSizeArg));
		RegisterDefault(MoveTemp(Tool));
	}

	{
		FHCIAbilityKitToolDescriptor Tool;
		Tool.ToolName = TEXT("SetMeshLODGroup");
		Tool.Capability = EHCIAbilityKitToolCapability::Write;
		Tool.bSupportsDryRun = true;
		Tool.bSupportsUndo = true;
		Tool.bDestructive = false;
		Tool.Domain = TEXT("AssetCompliance");
		Tool.Summary = TEXT("Assign mesh LOD group using frozen whitelist.");

		Tool.ArgsSchema.Add(MakeStringArrayArg(TEXT("asset_paths"), 1, 50));

		FHCIAbilityKitToolArgSchema LodGroupArg = MakeStringArg(TEXT("lod_group"));
		LodGroupArg.AllowedStringValues = {
			TEXT("LevelArchitecture"),
			TEXT("SmallProp"),
			TEXT("LargeProp"),
			TEXT("Foliage"),
			TEXT("Character")};
		Tool.ArgsSchema.Add(MoveTemp(LodGroupArg));
		RegisterDefault(MoveTemp(Tool));
	}

	{
		FHCIAbilityKitToolDescriptor Tool;
		Tool.ToolName = TEXT("ScanLevelMeshRisks");
		Tool.Capability = EHCIAbilityKitToolCapability::ReadOnly;
		Tool.bSupportsDryRun = false;
		Tool.bSupportsUndo = false;
		Tool.bDestructive = false;
		Tool.Domain = TEXT("LevelRisk");
		Tool.Summary = TEXT("Scan selected/all level actors for missing collision and default material risks.");

		FHCIAbilityKitToolArgSchema ScopeArg = MakeStringArg(TEXT("scope"));
		ScopeArg.AllowedStringValues = {TEXT("selected"), TEXT("all")};
		Tool.ArgsSchema.Add(MoveTemp(ScopeArg));

		FHCIAbilityKitToolArgSchema ChecksArg = MakeStringArrayArg(TEXT("checks"), 1, 2);
		ChecksArg.AllowedStringValues = {TEXT("missing_collision"), TEXT("default_material")};
		ChecksArg.bStringArrayAllowsSubsetOfEnum = true;
		Tool.ArgsSchema.Add(MoveTemp(ChecksArg));

		FHCIAbilityKitToolArgSchema MaxActorCountArg = MakeIntArg(TEXT("max_actor_count"));
		MaxActorCountArg.MinIntValue = 1;
		MaxActorCountArg.MaxIntValue = 5000;
		Tool.ArgsSchema.Add(MoveTemp(MaxActorCountArg));
		RegisterDefault(MoveTemp(Tool));
	}

	{
		FHCIAbilityKitToolDescriptor Tool;
		Tool.ToolName = TEXT("NormalizeAssetNamingByMetadata");
		Tool.Capability = EHCIAbilityKitToolCapability::Write;
		Tool.bSupportsDryRun = true;
		Tool.bSupportsUndo = true;
		Tool.bDestructive = false;
		Tool.Domain = TEXT("NamingTraceability");
		Tool.Summary = TEXT("Generate rename/move proposals from import metadata and naming policy.");

		Tool.ArgsSchema.Add(MakeStringArrayArg(TEXT("asset_paths"), 1, 50));

		FHCIAbilityKitToolArgSchema MetadataSourceArg = MakeStringArg(TEXT("metadata_source"));
		MetadataSourceArg.AllowedStringValues = {TEXT("auto"), TEXT("UAssetImportData"), TEXT("AssetUserData")};
		Tool.ArgsSchema.Add(MoveTemp(MetadataSourceArg));

		FHCIAbilityKitToolArgSchema PrefixModeArg = MakeStringArg(TEXT("prefix_mode"));
		PrefixModeArg.AllowedStringValues = {TEXT("auto_by_asset_class")};
		Tool.ArgsSchema.Add(MoveTemp(PrefixModeArg));

		FHCIAbilityKitToolArgSchema TargetRootArg = MakeStringArg(TEXT("target_root"));
		TargetRootArg.bMustStartWithGamePath = true;
		Tool.ArgsSchema.Add(MoveTemp(TargetRootArg));
		RegisterDefault(MoveTemp(Tool));
	}

	{
		FHCIAbilityKitToolDescriptor Tool;
		Tool.ToolName = TEXT("RenameAsset");
		Tool.Capability = EHCIAbilityKitToolCapability::Write;
		Tool.bSupportsDryRun = true;
		Tool.bSupportsUndo = true;
		Tool.bDestructive = false;
		Tool.Domain = TEXT("NamingTraceability");
		Tool.Summary = TEXT("Rename a single asset with strict identifier regex.");

		FHCIAbilityKitToolArgSchema AssetPathArg = MakeStringArg(TEXT("asset_path"));
		AssetPathArg.bMustStartWithGamePath = true;
		Tool.ArgsSchema.Add(MoveTemp(AssetPathArg));

		FHCIAbilityKitToolArgSchema NewNameArg = MakeStringArg(TEXT("new_name"));
		NewNameArg.MinStringLength = 1;
		NewNameArg.MaxStringLength = 64;
		NewNameArg.RegexPattern = TEXT("^[A-Za-z0-9_]+$");
		Tool.ArgsSchema.Add(MoveTemp(NewNameArg));
		RegisterDefault(MoveTemp(Tool));
	}

	{
		FHCIAbilityKitToolDescriptor Tool;
		Tool.ToolName = TEXT("MoveAsset");
		Tool.Capability = EHCIAbilityKitToolCapability::Write;
		Tool.bSupportsDryRun = true;
		Tool.bSupportsUndo = true;
		Tool.bDestructive = false;
		Tool.Domain = TEXT("NamingTraceability");
		Tool.Summary = TEXT("Move a single asset to a validated /Game path.");

		FHCIAbilityKitToolArgSchema AssetPathArg = MakeStringArg(TEXT("asset_path"));
		AssetPathArg.bMustStartWithGamePath = true;
		Tool.ArgsSchema.Add(MoveTemp(AssetPathArg));

		FHCIAbilityKitToolArgSchema TargetPathArg = MakeStringArg(TEXT("target_path"));
		TargetPathArg.bMustStartWithGamePath = true;
		Tool.ArgsSchema.Add(MoveTemp(TargetPathArg));
		RegisterDefault(MoveTemp(Tool));
	}
}

bool FHCIAbilityKitToolRegistry::RegisterTool(const FHCIAbilityKitToolDescriptor& InDescriptor, FString* OutError)
{
	if (InDescriptor.ToolName.IsNone())
	{
		if (OutError)
		{
			*OutError = TEXT("tool_name is empty");
		}
		return false;
	}

	if (ToolIndexByName.Contains(InDescriptor.ToolName))
	{
		if (OutError)
		{
			*OutError = FString::Printf(TEXT("duplicate tool_name: %s"), *InDescriptor.ToolName.ToString());
		}
		return false;
	}

	if (InDescriptor.bDestructive && InDescriptor.Capability != EHCIAbilityKitToolCapability::Destructive)
	{
		if (OutError)
		{
			*OutError = FString::Printf(TEXT("destructive tool must declare destructive capability: %s"), *InDescriptor.ToolName.ToString());
		}
		return false;
	}

	TSet<FName> ArgNames;
	for (const FHCIAbilityKitToolArgSchema& Arg : InDescriptor.ArgsSchema)
	{
		if (Arg.ArgName.IsNone())
		{
			if (OutError)
			{
				*OutError = FString::Printf(TEXT("tool %s has empty arg name"), *InDescriptor.ToolName.ToString());
			}
			return false;
		}

		if (ArgNames.Contains(Arg.ArgName))
		{
			if (OutError)
			{
				*OutError = FString::Printf(TEXT("tool %s has duplicate arg: %s"), *InDescriptor.ToolName.ToString(), *Arg.ArgName.ToString());
			}
			return false;
		}

		ArgNames.Add(Arg.ArgName);
	}

	ToolIndexByName.Add(InDescriptor.ToolName, Tools.Num());
	Tools.Add(InDescriptor);
	return true;
}

const FHCIAbilityKitToolDescriptor* FHCIAbilityKitToolRegistry::FindTool(const FName ToolName) const
{
	EnsureDefaultTools();

	const int32* Index = ToolIndexByName.Find(ToolName);
	if (!Index || !Tools.IsValidIndex(*Index))
	{
		return nullptr;
	}

	return &Tools[*Index];
}

bool FHCIAbilityKitToolRegistry::IsWhitelistedTool(const FName ToolName) const
{
	return FindTool(ToolName) != nullptr;
}

TArray<FHCIAbilityKitToolDescriptor> FHCIAbilityKitToolRegistry::GetAllTools() const
{
	EnsureDefaultTools();
	return Tools;
}

TArray<FName> FHCIAbilityKitToolRegistry::GetRegisteredToolNames() const
{
	EnsureDefaultTools();

	TArray<FName> Names;
	Names.Reserve(Tools.Num());
	for (const FHCIAbilityKitToolDescriptor& Tool : Tools)
	{
		Names.Add(Tool.ToolName);
	}
	return Names;
}

bool FHCIAbilityKitToolRegistry::ValidateFrozenDefaults(FString& OutError) const
{
	EnsureDefaultTools();

	for (const FHCIAbilityKitToolDescriptor& Tool : Tools)
	{
		if (Tool.ToolName.IsNone())
		{
			OutError = TEXT("tool_name is empty");
			return false;
		}

		if (Tool.Capability == EHCIAbilityKitToolCapability::ReadOnly && Tool.bDestructive)
		{
			OutError = FString::Printf(TEXT("read_only tool cannot be destructive: %s"), *Tool.ToolName.ToString());
			return false;
		}

		if (Tool.Capability == EHCIAbilityKitToolCapability::ReadOnly && Tool.bSupportsUndo)
		{
			OutError = FString::Printf(TEXT("read_only tool should not declare supports_undo=true: %s"), *Tool.ToolName.ToString());
			return false;
		}

		for (const FHCIAbilityKitToolArgSchema& Arg : Tool.ArgsSchema)
		{
			if (Arg.ArgName.IsNone())
			{
				OutError = FString::Printf(TEXT("tool %s contains empty arg name"), *Tool.ToolName.ToString());
				return false;
			}

			if (Arg.MinArrayLength != INDEX_NONE && Arg.MaxArrayLength != INDEX_NONE && Arg.MinArrayLength > Arg.MaxArrayLength)
			{
				OutError = FString::Printf(TEXT("tool %s arg %s has invalid array length bounds"), *Tool.ToolName.ToString(), *Arg.ArgName.ToString());
				return false;
			}

			if (Arg.MinIntValue != INDEX_NONE && Arg.MaxIntValue != INDEX_NONE && Arg.MinIntValue > Arg.MaxIntValue)
			{
				OutError = FString::Printf(TEXT("tool %s arg %s has invalid int bounds"), *Tool.ToolName.ToString(), *Arg.ArgName.ToString());
				return false;
			}

			if (Arg.MinStringLength != INDEX_NONE && Arg.MaxStringLength != INDEX_NONE && Arg.MinStringLength > Arg.MaxStringLength)
			{
				OutError = FString::Printf(TEXT("tool %s arg %s has invalid string length bounds"), *Tool.ToolName.ToString(), *Arg.ArgName.ToString());
				return false;
			}
		}
	}

	OutError.Reset();
	return true;
}

FString FHCIAbilityKitToolRegistry::CapabilityToString(const EHCIAbilityKitToolCapability Capability)
{
	switch (Capability)
	{
	case EHCIAbilityKitToolCapability::ReadOnly:
		return TEXT("read_only");
	case EHCIAbilityKitToolCapability::Write:
		return TEXT("write");
	case EHCIAbilityKitToolCapability::Destructive:
		return TEXT("destructive");
	default:
		return TEXT("unknown");
	}
}

FString FHCIAbilityKitToolRegistry::ArgValueTypeToString(const EHCIAbilityKitToolArgValueType ValueType)
{
	switch (ValueType)
	{
	case EHCIAbilityKitToolArgValueType::String:
		return TEXT("string");
	case EHCIAbilityKitToolArgValueType::StringArray:
		return TEXT("string[]");
	case EHCIAbilityKitToolArgValueType::Int:
		return TEXT("int");
	default:
		return TEXT("unknown");
	}
}

void FHCIAbilityKitToolRegistry::EnsureDefaultTools() const
{
	if (bDefaultsInitialized)
	{
		return;
	}

	const_cast<FHCIAbilityKitToolRegistry*>(this)->ResetToDefaults();
}

