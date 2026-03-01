#include "Agent/Tools/HCIToolRegistry.h"

namespace
{
static FHCIToolArgSchema MakeStringArrayArg(const TCHAR* ArgName, const int32 MinLen, const int32 MaxLen)
{
	FHCIToolArgSchema Arg;
	Arg.ArgName = FName(ArgName);
	Arg.ValueType = EHCIToolArgValueType::StringArray;
	Arg.MinArrayLength = MinLen;
	Arg.MaxArrayLength = MaxLen;
	return Arg;
}

static FHCIToolArgSchema MakeStringArg(const TCHAR* ArgName)
{
	FHCIToolArgSchema Arg;
	Arg.ArgName = FName(ArgName);
	Arg.ValueType = EHCIToolArgValueType::String;
	return Arg;
}

static FHCIToolArgSchema MakeIntArg(const TCHAR* ArgName)
{
	FHCIToolArgSchema Arg;
	Arg.ArgName = FName(ArgName);
	Arg.ValueType = EHCIToolArgValueType::Int;
	return Arg;
}
}

FHCIToolRegistry& FHCIToolRegistry::Get()
{
	static FHCIToolRegistry Instance;
	return Instance;
}

const FHCIToolRegistry& FHCIToolRegistry::GetReadOnly()
{
	const FHCIToolRegistry& Registry = Get();
	Registry.EnsureDefaultTools();
	return Registry;
}

void FHCIToolRegistry::ResetToDefaults()
{
	Tools.Reset();
	ToolIndexByName.Reset();
	bDefaultsInitialized = true;

	auto RegisterDefault = [this](FHCIToolDescriptor&& Descriptor)
	{
		FString Error;
		RegisterTool(Descriptor, &Error);
	};

	{
		FHCIToolDescriptor Tool;
		Tool.ToolName = TEXT("ScanAssets");
		Tool.Capability = EHCIToolCapability::ReadOnly;
		Tool.bSupportsDryRun = false;
		Tool.bSupportsUndo = false;
		Tool.bDestructive = false;
		Tool.Domain = TEXT("AssetCompliance");
		Tool.Summary = TEXT("Read-only asset audit scan entry point.");

		FHCIToolArgSchema DirectoryArg = MakeStringArg(TEXT("directory"));
		DirectoryArg.bRequired = false;
		DirectoryArg.bMustStartWithGamePath = true;
		Tool.ArgsSchema.Add(MoveTemp(DirectoryArg));
		RegisterDefault(MoveTemp(Tool));
	}

	{
		FHCIToolDescriptor Tool;
		Tool.ToolName = TEXT("ScanMeshTriangleCount");
		Tool.Capability = EHCIToolCapability::ReadOnly;
		Tool.bSupportsDryRun = false;
		Tool.bSupportsUndo = false;
		Tool.bDestructive = false;
		Tool.Domain = TEXT("AssetCompliance");
		Tool.Summary = TEXT("Read-only mesh triangle count scan by directory.");

		FHCIToolArgSchema DirectoryArg = MakeStringArg(TEXT("directory"));
		DirectoryArg.bRequired = false;
		DirectoryArg.bMustStartWithGamePath = true;
		Tool.ArgsSchema.Add(MoveTemp(DirectoryArg));
		RegisterDefault(MoveTemp(Tool));
	}

	{
		FHCIToolDescriptor Tool;
		Tool.ToolName = TEXT("SearchPath");
		Tool.Capability = EHCIToolCapability::ReadOnly;
		Tool.bSupportsDryRun = false;
		Tool.bSupportsUndo = false;
		Tool.bDestructive = false;
		Tool.Domain = TEXT("NamingTraceability");
		Tool.Summary = TEXT("Search candidate /Game folders by keyword for discovery-first planning.");

		FHCIToolArgSchema KeywordArg = MakeStringArg(TEXT("keyword"));
		KeywordArg.MinStringLength = 1;
		KeywordArg.MaxStringLength = 64;
		Tool.ArgsSchema.Add(MoveTemp(KeywordArg));
		RegisterDefault(MoveTemp(Tool));
	}

	{
		FHCIToolDescriptor Tool;
		Tool.ToolName = TEXT("SetTextureMaxSize");
		Tool.Capability = EHCIToolCapability::Write;
		Tool.bSupportsDryRun = true;
		Tool.bSupportsUndo = true;
		Tool.bDestructive = false;
		Tool.Domain = TEXT("AssetCompliance");
		Tool.Summary = TEXT("Set texture maximum size using frozen allowed enum values.");

		FHCIToolArgSchema AssetPathsArg = MakeStringArrayArg(TEXT("asset_paths"), 1, 50);
		AssetPathsArg.bRequiresPipelineInput = true;
		Tool.ArgsSchema.Add(MoveTemp(AssetPathsArg));

		FHCIToolArgSchema MaxSizeArg = MakeIntArg(TEXT("max_size"));
		MaxSizeArg.AllowedIntValues = {256, 512, 1024, 2048, 4096, 8192};
		Tool.ArgsSchema.Add(MoveTemp(MaxSizeArg));
		RegisterDefault(MoveTemp(Tool));
	}

	{
		FHCIToolDescriptor Tool;
		Tool.ToolName = TEXT("SetMeshLODGroup");
		Tool.Capability = EHCIToolCapability::Write;
		Tool.bSupportsDryRun = true;
		Tool.bSupportsUndo = true;
		Tool.bDestructive = false;
		Tool.Domain = TEXT("AssetCompliance");
		Tool.Summary = TEXT("Assign mesh LOD group using frozen whitelist.");

		FHCIToolArgSchema AssetPathsArg = MakeStringArrayArg(TEXT("asset_paths"), 1, 50);
		AssetPathsArg.bRequiresPipelineInput = true;
		Tool.ArgsSchema.Add(MoveTemp(AssetPathsArg));

		FHCIToolArgSchema LodGroupArg = MakeStringArg(TEXT("lod_group"));
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
		FHCIToolDescriptor Tool;
		Tool.ToolName = TEXT("ScanLevelMeshRisks");
		Tool.Capability = EHCIToolCapability::ReadOnly;
		Tool.bSupportsDryRun = false;
		Tool.bSupportsUndo = false;
		Tool.bDestructive = false;
		Tool.Domain = TEXT("LevelRisk");
		Tool.Summary = TEXT("Scan selected/all level actors for missing collision and default material risks.");

		FHCIToolArgSchema ScopeArg = MakeStringArg(TEXT("scope"));
		ScopeArg.AllowedStringValues = {TEXT("selected"), TEXT("all")};
		Tool.ArgsSchema.Add(MoveTemp(ScopeArg));

		FHCIToolArgSchema ChecksArg = MakeStringArrayArg(TEXT("checks"), 1, 2);
		ChecksArg.AllowedStringValues = {TEXT("missing_collision"), TEXT("default_material")};
		ChecksArg.bStringArrayAllowsSubsetOfEnum = true;
		Tool.ArgsSchema.Add(MoveTemp(ChecksArg));

		FHCIToolArgSchema MaxActorCountArg = MakeIntArg(TEXT("max_actor_count"));
		MaxActorCountArg.MinIntValue = 1;
		MaxActorCountArg.MaxIntValue = 5000;
		Tool.ArgsSchema.Add(MoveTemp(MaxActorCountArg));

		FHCIToolArgSchema ActorNamesArg = MakeStringArrayArg(TEXT("actor_names"), 1, 50);
		ActorNamesArg.bRequired = false;
		Tool.ArgsSchema.Add(MoveTemp(ActorNamesArg));
		RegisterDefault(MoveTemp(Tool));
	}

	{
		FHCIToolDescriptor Tool;
		Tool.ToolName = TEXT("NormalizeAssetNamingByMetadata");
		Tool.Capability = EHCIToolCapability::Write;
		Tool.bSupportsDryRun = true;
		Tool.bSupportsUndo = true;
		Tool.bDestructive = false;
		Tool.Domain = TEXT("NamingTraceability");
		Tool.Summary = TEXT("Generate rename/move proposals from import metadata and naming policy.");

		Tool.ArgsSchema.Add(MakeStringArrayArg(TEXT("asset_paths"), 1, 50));

		FHCIToolArgSchema MetadataSourceArg = MakeStringArg(TEXT("metadata_source"));
		MetadataSourceArg.AllowedStringValues = {TEXT("auto"), TEXT("UAssetImportData"), TEXT("AssetUserData")};
		Tool.ArgsSchema.Add(MoveTemp(MetadataSourceArg));

		FHCIToolArgSchema PrefixModeArg = MakeStringArg(TEXT("prefix_mode"));
		PrefixModeArg.AllowedStringValues = {TEXT("auto_by_asset_class")};
		Tool.ArgsSchema.Add(MoveTemp(PrefixModeArg));

		FHCIToolArgSchema TargetRootArg = MakeStringArg(TEXT("target_root"));
		TargetRootArg.bMustStartWithGamePath = true;
		Tool.ArgsSchema.Add(MoveTemp(TargetRootArg));
		RegisterDefault(MoveTemp(Tool));
	}

	{
		FHCIToolDescriptor Tool;
		Tool.ToolName = TEXT("RenameAsset");
		Tool.Capability = EHCIToolCapability::Write;
		Tool.bSupportsDryRun = true;
		Tool.bSupportsUndo = true;
		Tool.bDestructive = false;
		Tool.Domain = TEXT("NamingTraceability");
		Tool.Summary = TEXT("Rename a single asset with strict identifier regex.");

		FHCIToolArgSchema AssetPathArg = MakeStringArg(TEXT("asset_path"));
		AssetPathArg.bMustStartWithGamePath = true;
		Tool.ArgsSchema.Add(MoveTemp(AssetPathArg));

		FHCIToolArgSchema NewNameArg = MakeStringArg(TEXT("new_name"));
		NewNameArg.MinStringLength = 1;
		NewNameArg.MaxStringLength = 64;
		NewNameArg.RegexPattern = TEXT("^[A-Za-z0-9_]+$");
		Tool.ArgsSchema.Add(MoveTemp(NewNameArg));
		RegisterDefault(MoveTemp(Tool));
	}

	{
		FHCIToolDescriptor Tool;
		Tool.ToolName = TEXT("MoveAsset");
		Tool.Capability = EHCIToolCapability::Write;
		Tool.bSupportsDryRun = true;
		Tool.bSupportsUndo = true;
		Tool.bDestructive = false;
		Tool.Domain = TEXT("NamingTraceability");
		Tool.Summary = TEXT("Move a single asset to a validated /Game path.");

		FHCIToolArgSchema AssetPathArg = MakeStringArg(TEXT("asset_path"));
		AssetPathArg.bMustStartWithGamePath = true;
		Tool.ArgsSchema.Add(MoveTemp(AssetPathArg));

		FHCIToolArgSchema TargetPathArg = MakeStringArg(TEXT("target_path"));
		TargetPathArg.bMustStartWithGamePath = true;
		Tool.ArgsSchema.Add(MoveTemp(TargetPathArg));
		RegisterDefault(MoveTemp(Tool));
	}

	{
		FHCIToolDescriptor Tool;
		Tool.ToolName = TEXT("AutoMaterialSetupByNameContract");
		Tool.Capability = EHCIToolCapability::Write;
		Tool.bSupportsDryRun = true;
		Tool.bSupportsUndo = true;
		Tool.bDestructive = false;
		Tool.Domain = TEXT("MaterialSetup");
		Tool.Summary = TEXT("Create MaterialInstance(s) and bind BC/N/ORM textures by strict name contract, then assign to StaticMesh Slot0 (requires confirm).");

		FHCIToolArgSchema AssetPathsArg = MakeStringArrayArg(TEXT("asset_paths"), 1, 50);
		AssetPathsArg.bRequiresPipelineInput = true;
		Tool.ArgsSchema.Add(MoveTemp(AssetPathsArg));

		FHCIToolArgSchema TargetRootArg = MakeStringArg(TEXT("target_root"));
		TargetRootArg.bMustStartWithGamePath = true;
		Tool.ArgsSchema.Add(MoveTemp(TargetRootArg));

		FHCIToolArgSchema MasterMaterialArg = MakeStringArg(TEXT("master_material_path"));
		MasterMaterialArg.bRequired = false;
		MasterMaterialArg.bMustStartWithGamePath = true;
		Tool.ArgsSchema.Add(MoveTemp(MasterMaterialArg));

		RegisterDefault(MoveTemp(Tool));
	}
}

bool FHCIToolRegistry::RegisterTool(const FHCIToolDescriptor& InDescriptor, FString* OutError)
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

	if (InDescriptor.bDestructive && InDescriptor.Capability != EHCIToolCapability::Destructive)
	{
		if (OutError)
		{
			*OutError = FString::Printf(TEXT("destructive tool must declare destructive capability: %s"), *InDescriptor.ToolName.ToString());
		}
		return false;
	}

	TSet<FName> ArgNames;
	for (const FHCIToolArgSchema& Arg : InDescriptor.ArgsSchema)
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

const FHCIToolDescriptor* FHCIToolRegistry::FindTool(const FName ToolName) const
{
	EnsureDefaultTools();

	const int32* Index = ToolIndexByName.Find(ToolName);
	if (!Index || !Tools.IsValidIndex(*Index))
	{
		return nullptr;
	}

	return &Tools[*Index];
}

bool FHCIToolRegistry::IsWhitelistedTool(const FName ToolName) const
{
	return FindTool(ToolName) != nullptr;
}

TArray<FHCIToolDescriptor> FHCIToolRegistry::GetAllTools() const
{
	EnsureDefaultTools();
	return Tools;
}

TArray<FName> FHCIToolRegistry::GetRegisteredToolNames() const
{
	EnsureDefaultTools();

	TArray<FName> Names;
	Names.Reserve(Tools.Num());
	for (const FHCIToolDescriptor& Tool : Tools)
	{
		Names.Add(Tool.ToolName);
	}
	return Names;
}

bool FHCIToolRegistry::ValidateFrozenDefaults(FString& OutError) const
{
	EnsureDefaultTools();

	for (const FHCIToolDescriptor& Tool : Tools)
	{
		if (Tool.ToolName.IsNone())
		{
			OutError = TEXT("tool_name is empty");
			return false;
		}

		if (Tool.Capability == EHCIToolCapability::ReadOnly && Tool.bDestructive)
		{
			OutError = FString::Printf(TEXT("read_only tool cannot be destructive: %s"), *Tool.ToolName.ToString());
			return false;
		}

		if (Tool.Capability == EHCIToolCapability::ReadOnly && Tool.bSupportsUndo)
		{
			OutError = FString::Printf(TEXT("read_only tool should not declare supports_undo=true: %s"), *Tool.ToolName.ToString());
			return false;
		}

		for (const FHCIToolArgSchema& Arg : Tool.ArgsSchema)
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

FString FHCIToolRegistry::CapabilityToString(const EHCIToolCapability Capability)
{
	switch (Capability)
	{
	case EHCIToolCapability::ReadOnly:
		return TEXT("read_only");
	case EHCIToolCapability::Write:
		return TEXT("write");
	case EHCIToolCapability::Destructive:
		return TEXT("destructive");
	default:
		return TEXT("unknown");
	}
}

FString FHCIToolRegistry::ArgValueTypeToString(const EHCIToolArgValueType ValueType)
{
	switch (ValueType)
	{
	case EHCIToolArgValueType::String:
		return TEXT("string");
	case EHCIToolArgValueType::StringArray:
		return TEXT("string[]");
	case EHCIToolArgValueType::Int:
		return TEXT("int");
	default:
		return TEXT("unknown");
	}
}

void FHCIToolRegistry::EnsureDefaultTools() const
{
	if (bDefaultsInitialized)
	{
		return;
	}

	const_cast<FHCIToolRegistry*>(this)->ResetToDefaults();
}

