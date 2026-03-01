# StageH-SliceH5.2_证据白名单与管道断裂阻断（已通过）

- 日期：2026-02-26
- 负责人：Codex + 用户 UE 手测
- 目标：
  - 用例 C：验证 `expected_evidence` 非法键白名单拦截。
  - 用例 D：验证 `SearchPath` 管道断裂由 `W5101` 升级阻断为 `E4009`。

## 1. 测试步骤

1. 用例 C：`HCI.AgentPlanValidateDemo fail_illegal_evidence_key`
2. 用例 D：`HCI.Editor.AgentExecutor.PipelineBypassAfterSearchBlockedWithE4009`

## 2. 预期结果

1. 用例 C：
   - `error_code=E4009`
   - `detail=Illegal evidence key "unknown_key" for tool "ScanAssets".`
2. 用例 D：
   - `warning_code=W5101`
   - `error_code=E4009`
   - `detail` 包含：`Pipe variable from Step 1 (SearchPath) is defined but not consumed by subsequent steps`

## 3. 实际结果

1. 用例 C 命中：
   - `error_code=E4009`
   - `detail=Illegal evidence key "unknown_key" for tool "ScanAssets".`
2. 用例 D 命中：
   - `warning_code=W5101`
   - `error_code=E4009`
   - `detail=Pipe variable from Step 1 (SearchPath) is defined but not consumed by subsequent steps...`

## 4. 证据

- `LogHCIAbilityKitAgentPlanValidator: Warning: [HCIAbilityKit][AgentPlanValidator] error_code=E4009 field=steps[0].expected_evidence[0] reason=expected_evidence_not_allowed_for_tool detail=Illegal evidence key "unknown_key" for tool "ScanAssets".`
- `LogHCIAbilityKitAgentDemo: Display: [HCIAbilityKit][AgentPlanValidate] case=custom ... valid=false error_code=E4009 ... reason=Illegal evidence key "unknown_key" for tool "ScanAssets". ...`
- `LogHCIAbilityKitAgentExecutor: [HCIAbilityKit][AgentExecutor] error_code=E4009 warning_code=W5101 step_id=step_2_scan tool=ScanAssets reason=planner_pipeline_variable_not_used_after_search detail=Pipe variable from Step 1 (SearchPath) is defined but not consumed by subsequent steps. arg=directory expected_template={{step_1_search.matched_directories[0]}} raw_value=/Game/Temp`

## 5. 结论

- `Pass`（2026-02-26）。
- H5.2 门禁目标达成：非法证据键与管道断裂场景均具备稳定阻断和可定位日志。
