#if WITH_DEV_AUTOMATION_TESTS

#include "Agent/LLM/HCIAgentLlmClient.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
static bool HCI_WriteUtf8File(const FString& FilePath, const FString& Content)
{
	const FString Directory = FPaths::GetPath(FilePath);
	IFileManager::Get().MakeDirectory(*Directory, true);
	return FFileHelper::SaveStringToFile(Content, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

static FString HCI_NormalizeJsonPath(const FString& InPath)
{
	return InPath.Replace(TEXT("\\"), TEXT("/"));
}

static void HCI_DeleteDirRecursively(const FString& DirPath)
{
	IFileManager::Get().DeleteDirectory(*DirPath, false, true);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCILlmRouterDropsBelow50SuccessRateTest,
	"HCI.Editor.AgentPlanLLM.RouterDropsModelsBelow50PercentSuccessRate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCILlmRouterDropsBelow50SuccessRateTest::RunTest(const FString& Parameters)
{
	FHCIAgentLlmClient::ResetRoutingStateForTesting();

	const FString BaseDir = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("Automation/HCI/LlmRouterDropsBelow50"));
	const FString ProviderPath = BaseDir / TEXT("llm_provider.local.json");
	const FString RouterPath = BaseDir / TEXT("llm_router.local.json");
	const FString JsonRouterPath = HCI_NormalizeJsonPath(RouterPath);

	const FString ProviderJson = FString::Printf(
		TEXT("{\n")
		TEXT("  \"api_key\": \"unit-test-key\",\n")
		TEXT("  \"api_url\": \"https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions\",\n")
		TEXT("  \"model\": \"fallback-model\",\n")
		TEXT("  \"router_config_path\": \"%s\"\n")
		TEXT("}\n"),
		*JsonRouterPath);

	const FString RouterJson =
		TEXT("{\n")
		TEXT("  \"enabled\": true,\n")
		TEXT("  \"strategy\": \"smooth_wrr\",\n")
		TEXT("  \"min_success_rate\": 0.5,\n")
		TEXT("  \"models\": [\n")
		TEXT("    {\"model\": \"bad_model\", \"weight\": 100.0, \"success_rate\": 0.33},\n")
		TEXT("    {\"model\": \"good_model\", \"weight\": 1.0, \"success_rate\": 1.0}\n")
		TEXT("  ]\n")
		TEXT("}\n");

	TestTrue(TEXT("Write provider config"), HCI_WriteUtf8File(ProviderPath, ProviderJson));
	TestTrue(TEXT("Write router config"), HCI_WriteUtf8File(RouterPath, RouterJson));

	bool bAllGoodModel = true;
	for (int32 Index = 0; Index < 5; ++Index)
	{
		FHCIAgentLlmProviderConfig Config;
		FString Error;
		const bool bLoaded = FHCIAgentLlmClient::LoadProviderConfigFromJsonFile(
			ProviderPath,
			TEXT("https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions"),
			TEXT("qwen3.5-plus"),
			Config,
			Error);
		TestTrue(TEXT("LoadProviderConfigFromJsonFile should succeed"), bLoaded);
		if (!bLoaded)
		{
			AddError(FString::Printf(TEXT("Load failed: %s"), *Error));
			bAllGoodModel = false;
			break;
		}
		if (!Config.Model.Equals(TEXT("good_model"), ESearchCase::CaseSensitive))
		{
			bAllGoodModel = false;
		}
	}

	TestTrue(TEXT("Models below 50% success_rate should be dropped"), bAllGoodModel);

	HCI_DeleteDirRecursively(BaseDir);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHCILlmRouterSmoothWrrDistributionTest,
	"HCI.Editor.AgentPlanLLM.RouterUsesSmoothWeightedRoundRobin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHCILlmRouterSmoothWrrDistributionTest::RunTest(const FString& Parameters)
{
	FHCIAgentLlmClient::ResetRoutingStateForTesting();

	const FString BaseDir = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("Automation/HCI/LlmRouterSmoothWrr"));
	const FString ProviderPath = BaseDir / TEXT("llm_provider.local.json");
	const FString RouterPath = BaseDir / TEXT("llm_router.local.json");
	const FString JsonRouterPath = HCI_NormalizeJsonPath(RouterPath);

	const FString ProviderJson = FString::Printf(
		TEXT("{\n")
		TEXT("  \"api_key\": \"unit-test-key\",\n")
		TEXT("  \"model\": \"fallback-model\",\n")
		TEXT("  \"router_config_path\": \"%s\"\n")
		TEXT("}\n"),
		*JsonRouterPath);

	const FString RouterJson =
		TEXT("{\n")
		TEXT("  \"enabled\": true,\n")
		TEXT("  \"strategy\": \"smooth_wrr\",\n")
		TEXT("  \"min_success_rate\": 0.5,\n")
		TEXT("  \"models\": [\n")
		TEXT("    {\"model\": \"model_A\", \"weight\": 2.0, \"success_rate\": 1.0},\n")
		TEXT("    {\"model\": \"model_B\", \"weight\": 1.0, \"success_rate\": 1.0}\n")
		TEXT("  ]\n")
		TEXT("}\n");

	TestTrue(TEXT("Write provider config"), HCI_WriteUtf8File(ProviderPath, ProviderJson));
	TestTrue(TEXT("Write router config"), HCI_WriteUtf8File(RouterPath, RouterJson));

	int32 CountA = 0;
	int32 CountB = 0;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		FHCIAgentLlmProviderConfig Config;
		FString Error;
		const bool bLoaded = FHCIAgentLlmClient::LoadProviderConfigFromJsonFile(
			ProviderPath,
			TEXT("https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions"),
			TEXT("qwen3.5-plus"),
			Config,
			Error);
		TestTrue(TEXT("LoadProviderConfigFromJsonFile should succeed"), bLoaded);
		if (!bLoaded)
		{
			AddError(FString::Printf(TEXT("Load failed: %s"), *Error));
			break;
		}

		if (Config.Model.Equals(TEXT("model_A"), ESearchCase::CaseSensitive))
		{
			++CountA;
		}
		else if (Config.Model.Equals(TEXT("model_B"), ESearchCase::CaseSensitive))
		{
			++CountB;
		}
	}

	TestEqual(TEXT("model_A should be selected 4/6"), CountA, 4);
	TestEqual(TEXT("model_B should be selected 2/6"), CountB, 2);

	HCI_DeleteDirRecursively(BaseDir);
	return true;
}

#endif
