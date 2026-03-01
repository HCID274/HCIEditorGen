# 06_架构设计：Planner 与 Executor 解耦蓝图（全局 Step 3）

> 切片：`Phase4-SliceP4-1_GlobalStep3_Blueprint`  
> 目标：在不改动任何既有业务语义的前提下，完成“规划（Planner）/执行（Executor）”的**清晰边界**与**核心接口草图**，为后续迁移切片提供稳定蓝图。  
> 限制：本切片**只做设计文档与接口草图**，不修改任何 `.cpp/.h`。  

---

## 1) 当前痛点与坏味道分析（Pain Points）

### 1.1 `HCIAbilityKitAgentPlanner.cpp` 的 SRP 违规耦合点

**Planner 当前同时承担了至少 6 类职责（高耦合、低内聚）：**

1. **Planner Provider 选择 + 路由策略**  
   - 在同一文件内处理 `prefer_llm / keyword_fallback`、`circuit breaker`、`retry`、`fallback_reason` 等运行时策略与指标统计。
2. **Prompt/Skill Bundle 的加载与拼装**  
   - 直接依赖 `FHCIAbilityKitAgentPromptBuilder`，并在 Planner 内决定 bundle 路径、模板文件名、tools_schema 文件名等（配置职责与业务职责交织）。
3. **LLM 供应商/HTTP 实现细节强耦合**  
   - 直接包含 `IHttpRequest/IHttpResponse`，在 Planner 内部创建并管理 HTTP 请求生命周期、超时 ticker、response 解析等 I/O 细节。
4. **Plan JSON 解析与契约校验**  
   - 既负责把 LLM 输出解析成 `FHCIAbilityKitAgentPlan`，又负责 `ValidateMinimalContract`/`ValidatePlan` 的调用策略。
5. **“准备消息/澄清消息”文案内嵌**（UI/文案与规划逻辑耦合）  
   - 例如“未识别指令时返回 prepared message-only clarify”的中文文案直接写在 Planner 逻辑分支中（职责泄漏）。
6. **测试/模拟模式与故障注入逻辑掺入主实现**  
   - `MockMode/FailAttempts/Timeout/InvalidJson/ContractInvalid/...` 等分支与真实链路共存，导致核心路径可读性下降，也让 Provider 的边界更模糊。

**坏味道总结：**
- “一个文件里既做策略，又做 I/O，又做解析，又做契约校验，又写文案，又含测试注入”——这是典型 God Function/God Module 前兆。

### 1.2 `HCIAbilityKitAgentExecutor.cpp` 的 SRP 违规耦合点

**Executor 当前同样承担了过多责任：**

1. **变量模板 `{{step_x.evidence}}` 的解析与替换逻辑嵌在执行主循环里**  
   - 例如 `HCI_ParseVariableTemplate / HCI_ResolveJsonValueInternal / HCI_ResolveStepArguments` 等静态函数与 Evidence 存储结构深度绑定。  
   - 这部分本质是一个“深模块”（Variable/Evidence Resolver），应从 Executor 拆出，形成可复用、可单测、接口极窄的服务。
2. **执行前校验策略与执行循环交织**  
   - 包含“整体验证 vs per-step 前缀验证（resolved prefix）”分支决策；这属于执行编排策略层，应该与“执行动作”解耦。
3. **Preflight Gate（RBAC/SourceControl/BlastRadius/Confirm/LOD Safety）与工具执行揉在一起**  
   - 虽然已有 `HCIAbilityKitAgentExecutionGate.h`，但 Executor 仍在主循环中直接评估 gate 并写入 StepResult。  
   - Gate 评估是“决策服务”，工具执行是“动作服务”，两者最好通过窄接口组合。
4. **Evidence 生成（模拟执行时填充 ExpectedEvidence）也内嵌在 Executor**  
   - 当前由 `HCI_BuildEvidenceValue` 等函数按工具名/参数直接拼装字符串证据，导致“证据格式约定”散落在执行器里。

**坏味道总结：**
- Executor 同时是“变量系统 + 校验系统 + gate 决策系统 + 执行编排系统 + 证据生成系统”的集合体，难以替换/扩展其中任一部分。

---

## 2) 核心抽象接口定义（Core Interfaces Sketch，仅草图）

> 目标：把“接口变窄（简单好用）/实现变深（复杂隐藏）”，并保证依赖方向单向：  
> `Planner` 只产出 `Plan`；`Executor` 只消费 `Plan` 并产出 `RunResult`；Resolver/Provider 通过接口注入。

### 2.1 Planner 侧：Provider 解耦（LLM / Rule / Keyword）

```cpp
// Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Agent/Planner/Interfaces/IHCIAbilityKitPlannerProvider.h
#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

struct FHCIAbilityKitAgentPlan;
struct FHCIAbilityKitAgentPlannerBuildOptions;
struct FHCIAbilityKitAgentPlannerResultMetadata;
class FHCIAbilityKitToolRegistry;

class IHCIAbilityKitPlannerProvider
{
public:
	virtual ~IHCIAbilityKitPlannerProvider() = default;

	// Deep module: caller 只提供输入文本与 registry/options，拿到 plan + metadata 即可。
	virtual bool BuildPlan(
		const FString& UserText,
		const FString& RequestId,
		const FHCIAbilityKitToolRegistry& ToolRegistry,
		const FHCIAbilityKitAgentPlannerBuildOptions& Options,
		FHCIAbilityKitAgentPlan& OutPlan,
		FString& OutRouteReason,
		FHCIAbilityKitAgentPlannerResultMetadata& OutMetadata,
		FString& OutError) = 0;

	virtual void BuildPlanAsync(
		const FString& UserText,
		const FString& RequestId,
		const FHCIAbilityKitToolRegistry& ToolRegistry,
		const FHCIAbilityKitAgentPlannerBuildOptions& Options,
		TFunction<void(bool /*bBuilt*/, FHCIAbilityKitAgentPlan /*Plan*/, FString /*RouteReason*/, FHCIAbilityKitAgentPlannerResultMetadata /*Metadata*/, FString /*Error*/)> OnComplete) = 0;

	// 用于 Router 的能力声明：Provider 是否需要网络、是否支持 async 等。
	virtual bool IsNetworkBacked() const = 0;
	virtual const TCHAR* GetName() const = 0; // e.g. "llm", "keyword_fallback"
};
```

```cpp
// Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Agent/Planner/Interfaces/IHCIAbilityKitPlannerRouter.h
#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

struct FHCIAbilityKitAgentPlannerBuildOptions;

class IHCIAbilityKitPlannerProvider;

class IHCIAbilityKitPlannerRouter
{
public:
	virtual ~IHCIAbilityKitPlannerRouter() = default;

	// Deep module: 把“prefer_llm / circuit breaker / retry / fallback strategy”收敛在 Router 内。
	virtual TSharedRef<IHCIAbilityKitPlannerProvider> SelectProvider(
		const FHCIAbilityKitAgentPlannerBuildOptions& Options) = 0;
};
```

> 说明：  
> - `FHCIAbilityKitAgentPlanner` 未来将退化为“Facade/Orchestrator”，只负责：`Router.SelectProvider -> Provider.BuildPlan -> Validator`，其内部不再直接包含 HTTP、Prompt 拼装等深细节。  

### 2.2 Executor 侧：Evidence/Variable Resolver 深模块（核心）

```cpp
// Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Agent/Executor/Interfaces/IHCIAbilityKitEvidenceResolver.h
#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class FJsonObject;
struct FHCIAbilityKitAgentPlanStep;

struct FHCIAbilityKitEvidenceResolveError
{
	FString ErrorCode; // e.g. "E4311"
	FString Reason;    // e.g. "variable_template_invalid"
	FString Detail;    // optional, for logs
};

// EvidenceContext 的内部组织由实现隐藏（Deep Module），Executor 不应假设其存储结构。
class IHCIAbilityKitEvidenceContextView
{
public:
	virtual ~IHCIAbilityKitEvidenceContextView() = default;
	virtual bool TryGetEvidenceValue(
		const FString& StepId,
		const FString& EvidenceKey,
		int32 OptionalIndex, // INDEX_NONE 表示无 index
		FString& OutValue) const = 0;
};

class IHCIAbilityKitEvidenceResolver
{
public:
	virtual ~IHCIAbilityKitEvidenceResolver() = default;

	// 只关心“是否包含模板”，避免 Executor 自己递归扫 JSON。
	virtual bool StepArgsMayContainTemplates(const TSharedPtr<FJsonObject>& Args) const = 0;

	// Deep module: 负责解析 "{{step_x.key}}"、数组展开、pipe-delimited 兼容等全部细节。
	virtual bool ResolveStepArgs(
		const FHCIAbilityKitAgentPlanStep& InStep,
		const IHCIAbilityKitEvidenceContextView& EvidenceContext,
		FHCIAbilityKitAgentPlanStep& OutResolvedStep,
		FHCIAbilityKitEvidenceResolveError& OutError) const = 0;
};
```

### 2.3 Executor 侧：ToolAction 注入点（隔离“执行动作”）

```cpp
// Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Agent/Executor/Interfaces/IHCIAbilityKitToolActionProvider.h
#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class IHCIAbilityKitAgentToolAction;

class IHCIAbilityKitToolActionProvider
{
public:
	virtual ~IHCIAbilityKitToolActionProvider() = default;
	virtual TSharedPtr<IHCIAbilityKitAgentToolAction> FindToolAction(const FName& ToolName) const = 0;
};
```

> 说明：  
> - `ToolActionProvider` 由 Editor 注入（Editor 侧拥有真实 ToolActions），Runtime Executor 只依赖抽象接口。  
> - 这能保证依赖方向继续保持 `Editor -> Runtime`（Editor 实现接口并注入），Runtime 不反向依赖 Editor。

---

## 3) 重构后的目录结构树（Target Directory Structure）

> 仅展示 Runtime 侧的目标结构（Editor 保持“提供 ToolActions 实现 + 注入 Provider”的角色）。

```text
Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/
|-- Public/
|   `-- Agent/
|       |-- Planner/
|       |   |-- Interfaces/
|       |   |   |-- IHCIAbilityKitPlannerProvider.h
|       |   |   `-- IHCIAbilityKitPlannerRouter.h
|       |   |-- Providers/                # 头文件（若需要公开类型）
|       |   |   |-- HCIAbilityKitKeywordPlannerProvider.h
|       |   |   `-- HCIAbilityKitLlmPlannerProvider.h
|       |   `-- HCIAbilityKitAgentPlannerFacade.h  # 未来：稳定门面（替代“巨型 .cpp 的静态方法集合”）
|       |
|       `-- Executor/
|           |-- Interfaces/
|           |   |-- IHCIAbilityKitEvidenceResolver.h
|           |   `-- IHCIAbilityKitToolActionProvider.h
|           |-- Services/
|           |   `-- HCIAbilityKitExecutionPipeline.h  # 未来：StepRunner + Gate + Validator 编排
|           `-- HCIAbilityKitAgentExecutorFacade.h
|
`-- Private/
    `-- Agent/
        |-- Planner/
        |   |-- Router/
        |   |   `-- HCIAbilityKitPlannerRouter_Default.cpp
        |   `-- Providers/
        |       |-- HCIAbilityKitKeywordPlannerProvider.cpp
        |       `-- HCIAbilityKitLlmPlannerProvider.cpp  # 内部持有 PromptBuilder + LlmClient
        |
        `-- Executor/
            |-- Resolver/
            |   `-- HCIAbilityKitEvidenceResolver_Default.cpp
            |-- Gates/
            |   `-- HCIAbilityKitExecutorPreflightService.cpp
            `-- Pipeline/
                `-- HCIAbilityKitExecutorPipeline.cpp
```

> 命名约束：  
> - 避免“扁平目录 + 超长前缀文件名”；用目录层级表达语义归属（与 Step 2 ToolActions 收敛一致）。  

---

## 4) 单向数据流向图（Data Flow）

### 4.1 高层单向流（无反向依赖）

```text
UserInput (string)
  |
  v
PlannerRouter (select provider; circuit/retry policy)  --(depends on config only)-->
  |
  v
PlannerProvider
  |-- KeywordProvider (pure rules; no network)
  `-- LlmProvider (PromptBuilder + LlmClient; network details hidden)
  |
  v
Plan (FHCIAbilityKitAgentPlan) + Metadata
  |
  v
ExecutorFacade
  |-- PlanValidator (contract + whitelist)
  |-- PreflightService (RBAC/SC/BlastRadius/Confirm/LOD; decision only)
  |-- EvidenceResolver (resolve {{step_x.evidence}}; deep module)
  |-- ToolActionProvider (Editor-injected) -> ToolAction.Execute/DryRun
  |
  v
RunResult (FHCIAbilityKitAgentExecutorRunResult)
  |
  v
UI (ChatUI/PreviewUI)  # UI 只消费结果，不参与 Planner/Executor 语义
```

### 4.2 关键“禁止反向依赖”的证明点

- `PlannerProvider` **不依赖** `Executor`、不读取执行结果；它只产出 Plan。  
- `EvidenceResolver` 是 Executor 的可替换部件，**不依赖** UI、HTTP、ToolAction 实现细节。  
- `ToolActionProvider` 的实现位于 Editor（拥有具体 ToolActions），Runtime 只依赖接口；因此依赖方向保持：`Editor -> Runtime`（注入），不会出现 `Runtime -> Editor`。

---

## 评审清单（你审核时建议关注）

1. Provider 边界是否足够窄：外部只看见 `BuildPlan/BuildPlanAsync`。  
2. EvidenceResolver 是否足够“深”：Executor 不再拥有 regex/递归解析/pipe list 展开等细节。  
3. Router 是否能承接现有的 retry/circuit breaker/metrics 责任，避免 Planner 再次膨胀。  
4. 是否保持契约冻结：`tool_name/args_schema/evidence keys/error codes` 在迁移阶段不变（只挪实现位置）。  

