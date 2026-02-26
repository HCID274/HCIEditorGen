# StageH-SliceH5.1_expected_evidence缺失契约校验（已通过）

- 日期：2026-02-26
- 负责人：Codex + 用户 UE 手测
- 目标：验证当 LLM 计划缺失 `expected_evidence` 时，系统是否稳定收敛到 `E4303/llm_contract_invalid`，并输出明确字段级诊断文案。

## 1. 测试步骤

1. UE 控制台执行：`HCIAbilityKit.AgentPlanWithLLMFallbackDemo contract_invalid 扫描关卡缺碰撞和默认材质`
2. 观察 Planner 与 AgentPlan 摘要日志。

## 2. 预期结果

1. 命中 `fallback_reason=llm_contract_invalid`。
2. 命中 `error_code=E4303`。
3. 日志 detail 明确包含：`Missing required field: expected_evidence`。
4. 回退后仍生成有效计划：`intent=scan_level_mesh_risks` 且 `plan_validation=ok`。

## 3. 实际结果

1. 命中：`fallback_reason=llm_contract_invalid`。
2. 命中：`error_code=E4303`。
3. 命中：`detail=Missing required field: expected_evidence`。
4. 命中回退计划：`intent=scan_level_mesh_risks`，且 `plan_validation=ok`。

## 4. 证据

- `LogHCIAbilityKitAgentPlanner: Display: [HCIAbilityKit][AgentPlanLLM][E4303] ... fallback_reason=llm_contract_invalid detail=Missing required field: expected_evidence ...`
- `LogHCIAbilityKitAgentDemoLlm: Display: [HCIAbilityKit][AgentPlanLLM] case=fallback ... intent=scan_level_mesh_risks ... fallback_reason=llm_contract_invalid error_code=E4303 ... plan_validation=ok ...`
- `LogHCIAbilityKitAgentDemoLlm: Display: [HCIAbilityKit][AgentPlan] case=fallback ... tool_name=ScanLevelMeshRisks ...`

## 5. 结论

- `Pass`（2026-02-26）。
- H5.1 本次门禁目标达成：`expected_evidence` 缺失场景已具备可观测、可定位、可回退的稳定行为。
