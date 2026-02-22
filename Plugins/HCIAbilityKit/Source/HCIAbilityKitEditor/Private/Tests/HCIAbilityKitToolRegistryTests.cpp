#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/HCIAbilityKitToolRegistry.h"
#include "Misc/AutomationTest.h"

namespace
{
static const FHCIAbilityKitToolArgSchema* FindArg(const FHCIAbilityKitToolDescriptor& Tool, const TCHAR* ArgName)
{
	return Tool.ArgsSchema.FindByPredicate(
		[ArgName](const FHCIAbilityKitToolArgSchema& Arg)
		{
			return Arg.ArgName == FName(ArgName);
		});
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitToolRegistryWhitelistFrozenTest,
	"HCIAbilityKit.Editor.AgentTools.RegistryWhitelistFrozen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitToolRegistryWhitelistFrozenTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	FString Error;
	TestTrue(TEXT("Frozen defaults should validate"), Registry.ValidateFrozenDefaults(Error));
	TestTrue(TEXT("Validation error should be empty"), Error.IsEmpty());

	const TArray<FName> ToolNames = Registry.GetRegisteredToolNames();
	TestEqual(TEXT("E1 whitelist should contain exactly 7 tools"), ToolNames.Num(), 7);

	const TArray<FName> ExpectedNames{
		TEXT("ScanAssets"),
		TEXT("SetTextureMaxSize"),
		TEXT("SetMeshLODGroup"),
		TEXT("ScanLevelMeshRisks"),
		TEXT("NormalizeAssetNamingByMetadata"),
		TEXT("RenameAsset"),
		TEXT("MoveAsset")};

	for (const FName& ExpectedName : ExpectedNames)
	{
		TestTrue(
			*FString::Printf(TEXT("Whitelist should contain %s"), *ExpectedName.ToString()),
			Registry.IsWhitelistedTool(ExpectedName));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitToolRegistryArgsSchemaFrozenTest,
	"HCIAbilityKit.Editor.AgentTools.ArgsSchemaFrozen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitToolRegistryArgsSchemaFrozenTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	const FHCIAbilityKitToolDescriptor* TextureTool = Registry.FindTool(TEXT("SetTextureMaxSize"));
	TestNotNull(TEXT("SetTextureMaxSize should exist"), TextureTool);
	if (!TextureTool)
	{
		return false;
	}

	const FHCIAbilityKitToolArgSchema* TexturePathsArg = FindArg(*TextureTool, TEXT("asset_paths"));
	TestNotNull(TEXT("SetTextureMaxSize.asset_paths should exist"), TexturePathsArg);
	if (TexturePathsArg)
	{
		TestEqual(TEXT("asset_paths type should be string[]"), static_cast<uint8>(TexturePathsArg->ValueType), static_cast<uint8>(EHCIAbilityKitToolArgValueType::StringArray));
		TestEqual(TEXT("asset_paths min length"), TexturePathsArg->MinArrayLength, 1);
		TestEqual(TEXT("asset_paths max length"), TexturePathsArg->MaxArrayLength, 50);
	}

	const FHCIAbilityKitToolArgSchema* MaxSizeArg = FindArg(*TextureTool, TEXT("max_size"));
	TestNotNull(TEXT("SetTextureMaxSize.max_size should exist"), MaxSizeArg);
	if (MaxSizeArg)
	{
		const TArray<int32> ExpectedSizes{256, 512, 1024, 2048, 4096, 8192};
		TestEqual(TEXT("max_size enum count"), MaxSizeArg->AllowedIntValues.Num(), ExpectedSizes.Num());
		for (int32 Index = 0; Index < ExpectedSizes.Num() && Index < MaxSizeArg->AllowedIntValues.Num(); ++Index)
		{
			TestEqual(
				*FString::Printf(TEXT("max_size enum value[%d]"), Index),
				MaxSizeArg->AllowedIntValues[Index],
				ExpectedSizes[Index]);
		}
	}

	const FHCIAbilityKitToolDescriptor* LodTool = Registry.FindTool(TEXT("SetMeshLODGroup"));
	TestNotNull(TEXT("SetMeshLODGroup should exist"), LodTool);
	if (!LodTool)
	{
		return false;
	}

	const FHCIAbilityKitToolArgSchema* LodGroupArg = FindArg(*LodTool, TEXT("lod_group"));
	TestNotNull(TEXT("SetMeshLODGroup.lod_group should exist"), LodGroupArg);
	if (LodGroupArg)
	{
		const TArray<FString> ExpectedLodGroups{
			TEXT("LevelArchitecture"),
			TEXT("SmallProp"),
			TEXT("LargeProp"),
			TEXT("Foliage"),
			TEXT("Character")};
		TestEqual(TEXT("lod_group enum count"), LodGroupArg->AllowedStringValues.Num(), ExpectedLodGroups.Num());
		for (int32 Index = 0; Index < ExpectedLodGroups.Num() && Index < LodGroupArg->AllowedStringValues.Num(); ++Index)
		{
			TestEqual(
				*FString::Printf(TEXT("lod_group enum value[%d]"), Index),
				LodGroupArg->AllowedStringValues[Index],
				ExpectedLodGroups[Index]);
		}
	}

	const FHCIAbilityKitToolDescriptor* LevelRiskTool = Registry.FindTool(TEXT("ScanLevelMeshRisks"));
	TestNotNull(TEXT("ScanLevelMeshRisks should exist"), LevelRiskTool);
	if (!LevelRiskTool)
	{
		return false;
	}

	const FHCIAbilityKitToolArgSchema* ScopeArg = FindArg(*LevelRiskTool, TEXT("scope"));
	const FHCIAbilityKitToolArgSchema* ChecksArg = FindArg(*LevelRiskTool, TEXT("checks"));
	const FHCIAbilityKitToolArgSchema* MaxActorCountArg = FindArg(*LevelRiskTool, TEXT("max_actor_count"));
	TestNotNull(TEXT("ScanLevelMeshRisks.scope should exist"), ScopeArg);
	TestNotNull(TEXT("ScanLevelMeshRisks.checks should exist"), ChecksArg);
	TestNotNull(TEXT("ScanLevelMeshRisks.max_actor_count should exist"), MaxActorCountArg);

	if (ScopeArg)
	{
		TestEqual(TEXT("scope enum count"), ScopeArg->AllowedStringValues.Num(), 2);
		TestEqual(TEXT("scope[0]"), ScopeArg->AllowedStringValues[0], FString(TEXT("selected")));
		TestEqual(TEXT("scope[1]"), ScopeArg->AllowedStringValues[1], FString(TEXT("all")));
	}

	if (ChecksArg)
	{
		TestTrue(TEXT("checks should allow subset-of-enum"), ChecksArg->bStringArrayAllowsSubsetOfEnum);
		TestEqual(TEXT("checks enum count"), ChecksArg->AllowedStringValues.Num(), 2);
	}

	if (MaxActorCountArg)
	{
		TestEqual(TEXT("max_actor_count min"), MaxActorCountArg->MinIntValue, 1);
		TestEqual(TEXT("max_actor_count max"), MaxActorCountArg->MaxIntValue, 5000);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCIAbilityKitToolRegistryDomainsAndFlagsTest,
	"HCIAbilityKit.Editor.AgentTools.DomainCoverageAndFlags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCIAbilityKitToolRegistryDomainsAndFlagsTest::RunTest(const FString& Parameters)
{
	FHCIAbilityKitToolRegistry& Registry = FHCIAbilityKitToolRegistry::Get();
	Registry.ResetToDefaults();

	const TArray<FHCIAbilityKitToolDescriptor> Tools = Registry.GetAllTools();
	TSet<FString> Domains;
	for (const FHCIAbilityKitToolDescriptor& Tool : Tools)
	{
		Domains.Add(Tool.Domain);
		if (Tool.Capability == EHCIAbilityKitToolCapability::ReadOnly)
		{
			TestFalse(
				*FString::Printf(TEXT("Read-only tool should not be destructive: %s"), *Tool.ToolName.ToString()),
				Tool.bDestructive);
		}
	}

	TestTrue(TEXT("AssetCompliance domain should be covered"), Domains.Contains(TEXT("AssetCompliance")));
	TestTrue(TEXT("LevelRisk domain should be covered"), Domains.Contains(TEXT("LevelRisk")));
	TestTrue(TEXT("NamingTraceability domain should be covered"), Domains.Contains(TEXT("NamingTraceability")));

	const FHCIAbilityKitToolDescriptor* RenameTool = Registry.FindTool(TEXT("RenameAsset"));
	TestNotNull(TEXT("RenameAsset should exist"), RenameTool);
	if (RenameTool)
	{
		TestEqual(
			TEXT("RenameAsset capability should be write"),
			FHCIAbilityKitToolRegistry::CapabilityToString(RenameTool->Capability),
			FString(TEXT("write")));
		TestTrue(TEXT("RenameAsset supports dry-run"), RenameTool->bSupportsDryRun);
		TestTrue(TEXT("RenameAsset supports undo"), RenameTool->bSupportsUndo);

		const FHCIAbilityKitToolArgSchema* NewNameArg = FindArg(*RenameTool, TEXT("new_name"));
		TestNotNull(TEXT("RenameAsset.new_name should exist"), NewNameArg);
		if (NewNameArg)
		{
			TestEqual(TEXT("new_name regex should be frozen"), NewNameArg->RegexPattern, FString(TEXT("^[A-Za-z0-9_]+$")));
			TestEqual(TEXT("new_name min length"), NewNameArg->MinStringLength, 1);
			TestEqual(TEXT("new_name max length"), NewNameArg->MaxStringLength, 64);
		}
	}

	return true;
}

#endif

