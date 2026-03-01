# 2026-02-25 StageH-SliceH4 LLM路由透明接管与加权轮询

- 日期：2026-02-25
- 切片编号：Stage H-SliceH4
- 测试人：用户 + 助手

## 1. 测试目标

- 将真实 LLM 调用切换到 Runtime 内部透明 Router，不改 Planner 上层接口。
- 验证 `<50%` 成功率模型被剔除。
- 验证到期权重驱动的 `smooth_wrr` 能在运行时生效。

## 2. 影响范围

- Runtime：`FHCIAbilityKitAgentLlmClient`（Provider 配置加载与模型选择）。
- Editor 自动化：`HCI.Editor.AgentPlanLLM.Router*`。
- 数据侧：`Saved/HCIAbilityKit/Config/llm_router.local.json`（由真实压测产出）。

## 3. 前置条件

- `Saved/HCIAbilityKit/Config/llm_provider.local.json` 配置可用。
- 可访问阿里云百炼 API。
- 17 模型测试脚本可执行。

## 4. 操作步骤

1. 执行 Python 压测：17 模型 * 每模型 3 次，生成中文报告与 `llm_router.local.json`。
2. 编译 `HCIEditorGenEditor Win64 Development`。
3. 执行 UE 自动化：`Automation RunTests HCI.Editor.AgentPlanLLM.Router`。
4. 执行真实探针：`HCI.AgentPlanWithRealLLMProbe "你是谁"`，检查路由日志与返回状态。

## 5. 预期结果

- 成功率 `<50%` 的模型不会进入路由池。
- 日志出现 `[HCIAbilityKit][LlmRouter] selected_model=...`。
- Probe 返回 `success=true status=200`。
- 自动化 2 条 Router 测试全绿。

## 6. 实际结果

- 路由配置已生成，`min_success_rate=0.5`，模型池 `pool=15`。
- 自动化命中并通过：
  - `RouterDropsModelsBelow50PercentSuccessRate`
  - `RouterUsesSmoothWeightedRoundRobin`
- 运行日志命中：
  - `[HCIAbilityKit][LlmRouter] selected_model=qwen-plus-2025-12-01 ... pool=15 ...`
  - Probe `success=true status=200`。
- 用户反馈：`Pass`。

## 7. 结论

- `Pass`

## 8. 证据

- 日志路径：`Saved/Logs/HCIEditorGen.log`
- 报告路径：`Source/HCIEditorGen/文档/测试记录/2026-02-25_百炼17模型API测试与智能路由报告.md`
- 路由配置：`Saved/HCIAbilityKit/Config/llm_router.local.json`

## 9. 问题与后续动作

- 暂无阻塞问题。
- 下一片待用户指定（按规则停止推进）。
