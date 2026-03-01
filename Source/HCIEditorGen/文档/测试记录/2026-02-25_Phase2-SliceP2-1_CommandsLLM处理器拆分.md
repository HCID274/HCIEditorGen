# 2026-02-25 Phase2-SliceP2-1 Commands LLM 处理器拆分（No Semantic Changes）

## 1. 目标

- 在不改变业务语义的前提下，继续拆解 `HCIAbilityKitAgentDemoConsoleCommands.cpp`。
- 将 LLM 相关命令处理实现下沉到独立文件，降低单文件复杂度并为 Phase2 后续拆分打基础。

## 2. 变更范围（冻结）

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Commands/HCIAbilityKitAgentCommandHandlers_LLM.cpp`（新增）
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Commands/HCIAbilityKitAgentDemoConsoleCommands.cpp`（删除已迁移实现）
- `Source/HCIEditorGen/文档/04_代码框架总览_树状图.md`
- `Source/HCIEditorGen/文档/05_开发执行总方案_资产审计.md`
- `Source/HCIEditorGen/文档/00_总进度.md`
- `AGENTS.md`

## 3. 实施步骤

1. 冻结切片边界：仅迁移命令处理实现，不改命令注册和接口。
2. 新增 `HCIAbilityKitAgentCommandHandlers_LLM.cpp`，承接 6 个命令处理函数：
   - `AgentPlanWithRealLLMProbe`
   - `AgentPlanWithLLMDemo`
   - `AgentPlanWithRealLLMDemo`
   - `AgentPlanWithLLMFallbackDemo`
   - `AgentPlanWithLLMStabilityDemo`
   - `AgentPlanWithLLMMetricsDump`
3. 从 `HCIAbilityKitAgentDemoConsoleCommands.cpp` 移除上述函数实现。
4. 运行全量编译门禁，确认无语义回归。

## 4. 本地验证

- 编译命令：
  - `D:\Unreal Engine\UE_5.4\Engine\Build\BatchFiles\Build.bat HCIEditorGenEditor Win64 Development D:\1_Projects\04_GameDev\UE_Projects\HCIEditorGen\HCIEditorGen.uproject -NoHotReloadFromIDE`
- 结果：
  - `Pass`（Exit Code 0）
  - 关键证据：新增文件与原巨文件均编译通过，Editor 链接完成。

## 5. UE 手测门禁（已通过）

1. `HCI.AgentPlanWithRealLLMProbe "你是谁"`
2. `HCI.AgentPlanWithRealLLMDemo "整理临时目录资产并归档"`
3. `HCI.AgentPlanWithLLMFallbackDemo timeout "整理临时目录资产并归档"`
4. `HCI.AgentPlanWithLLMMetricsDump`

## 6. UE 手测结果

- 结果：`Pass`
- 关键日志证据：
  - `AgentPlanWithRealLLMProbe`：`success=true status=200`，且命中路由日志 `[LlmRouter] selected_model=...`。
  - `AgentPlanWithRealLLMDemo`：命令正常异步返回并输出计划日志（出现 `llm_contract_invalid -> keyword_fallback`，属模型契约输出收敛，不是本次结构拆分回归）。
  - `AgentPlanWithLLMFallbackDemo timeout`：按预期触发 `fallback_reason=llm_timeout`。
  - `AgentPlanWithLLMMetricsDump`：指标正常输出。

## 7. 结论

- `Phase2-SliceP2-1` 关闭。
- 结论：在 `No Semantic Changes` 前提下，结构拆分通过编译与 UE 手测门禁。
