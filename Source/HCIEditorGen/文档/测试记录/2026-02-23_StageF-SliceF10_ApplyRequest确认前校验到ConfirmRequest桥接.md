# StageF-SliceF10 ApplyRequest 确认前校验 -> ConfirmRequest 执行确认申请桥接（2026-02-23）

- 日期：2026-02-23
- 切片编号：StageF-SliceF10
- 测试人：助手（实现/编译/自动化）+ 用户（UE 手测）
- 当前状态：已通过（用户 UE 手测 Pass）

## 1. 测试目标

- 基于 F9 的 `ApplyRequest` 与最新审阅预览执行确认前校验（`review_request_id + selection_digest + ready + user_confirmed`）。
- 桥接输出 `ConfirmRequest` 摘要/行日志与 JSON（仍为 `simulate_dry_run`，不触发真实资产写入）。
- 保持 F6/F7/F8/F9 命令与日志口径不回归。

## 2. 影响范围

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Agent/HCIAbilityKitAgentApplyConfirmRequest.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Agent/HCIAbilityKitAgentApplyConfirmRequestJsonSerializer.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Agent/HCIAbilityKitAgentExecutorApplyConfirmBridge.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Agent/HCIAbilityKitAgentApplyConfirmRequestJsonSerializer.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Agent/HCIAbilityKitAgentExecutorApplyConfirmBridge.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Commands/HCIAbilityKitAgentDemoConsoleCommands.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Commands/HCIAbilityKitAgentDemoConsoleCommands.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Tests/HCIAbilityKitAgentExecutorApplyConfirmTests.cpp`
- `Source/HCIEditorGen/文档/00_总进度.md`
- `Source/HCIEditorGen/文档/03_Schema_v1与错误契约.md`
- `Source/HCIEditorGen/文档/04_代码框架总览_树状图.md`
- `Source/HCIEditorGen/文档/05_开发执行总方案_资产审计.md`
- `AGENTS.md`

## 3. 前置条件

- `StageF-SliceF9` 已通过。
- 工程可编译。
- UE 编辑器可执行控制台命令。

## 4. 操作步骤（UE 手测）

1. 生成可通过的审阅预览（Actor）
   - `HCI.AgentExecutePlanReviewDemo ok_level_risk`
2. 选择单行（可选，确保使用 F8 采纳子集）
   - `HCI.AgentExecutePlanReviewSelect 0`
3. 生成 ApplyRequest（F9）
   - `HCI.AgentExecutePlanReviewPrepareApply`
4. 生成 ConfirmRequest（正常通过）
   - `HCI.AgentExecutePlanReviewPrepareConfirm 1 none`
5. 生成 ConfirmRequest JSON
   - `HCI.AgentExecutePlanReviewPrepareConfirmJson 1 none`
6. 未确认场景（应被确认前校验拦截）
   - `HCI.AgentExecutePlanReviewPrepareConfirm 0 none`
7. Digest 篡改场景（应命中 `E4202`）
   - `HCI.AgentExecutePlanReviewPrepareConfirm 1 digest`
8. 生成阻断型 ApplyRequest（F9，`ready=false`）
   - `HCI.AgentExecutePlanReviewDemo fail_confirm`
   - `HCI.AgentExecutePlanReviewPrepareApply`
9. 对阻断型 ApplyRequest 执行确认前校验（应命中 `E4203`）
   - `HCI.AgentExecutePlanReviewPrepareConfirm 1 none`

## 5. 预期结果（Pass 判定标准）

1. 合法命令无 `Error`
   - 步骤 1~9 在合法参数下无 `Error`（拦截场景出现 `Warning` 属预期）
2. `PrepareConfirm` 摘要日志存在且字段完整
   - `[HCIAbilityKit][AgentExecutorConfirm] summary ...`
   - 包含：`request_id/apply_request_id/review_request_id/selection_digest/user_confirmed/ready_to_execute/error_code/reason/validation=ok`
3. 正常通过场景（步骤 4）命中
   - `user_confirmed=true`
   - `ready_to_execute=true`
   - `error_code=-`
   - `reason=confirm_request_ready`
4. `PrepareConfirm` 行日志保留 F9 行级定位信息（正常场景）
   - `tool_name=ScanLevelMeshRisks`
   - `object_type=actor`
   - `locate_strategy=camera_focus`
   - `evidence_key=actor_path`
5. 未确认场景（步骤 6）命中
   - `user_confirmed=false`
   - `ready_to_execute=false`
   - `error_code=E4005`
   - `reason=user_not_confirmed`
6. Digest 篡改场景（步骤 7）命中
   - `ready_to_execute=false`
   - `error_code=E4202`
   - `reason=selection_digest_mismatch`
7. 阻断型 ApplyRequest 场景（步骤 9）命中
   - `ready_to_execute=false`
   - `error_code=E4203`
   - `reason=apply_request_not_ready_blocked_rows_present`
8. `PrepareConfirmJson` 输出 JSON 且包含
   - `"request_id"`
   - `"apply_request_id"`
   - `"review_request_id"`
   - `"selection_digest"`
   - `"user_confirmed"`
   - `"ready_to_execute"`
   - `"error_code"`
   - `"reason"`
   - `"summary"`
   - `"items"`
   - `"blocked"`
   - `"skip_reason"`
   - `"locate_strategy"`
9. F9/F8/F7/F6 回归不破坏
   - `ReviewDemo / ReviewSelect / PrepareApply` 仍可正常使用，日志前缀与字段口径不变

## 6. 实际结果（助手本地验证）

- 编译：通过
  - `Build.bat HCIEditorGenEditor Win64 Development ...` 成功。
- 自动化：通过（串行留证）
  - `HCI.Editor.AgentExecutorApplyConfirm`：5/5 成功（F10 新增）
    - `DigestMismatchReturnsE4202`
    - `JsonIncludesReadyAndValidationFields`
    - `NotReadyApplyReturnsE4203`
    - `ReadyWhenConfirmedAndMatchingState`
    - `UnconfirmedReturnsE4005`
  - `HCI.Editor.AgentExecutorApply`：通过（F9 回归；日志含 F9/F10 前缀测试结果）
  - `HCI.Editor.AgentExecutorReviewSelect`：3/3 成功（F8 回归）
- 留证日志：
  - `Saved/Logs/Automation_F10_AgentExecutorApplyConfirm_serial.log`
  - `Saved/Logs/Automation_F10_AgentExecutorApply_serial.log`
  - `Saved/Logs/Automation_F10_AgentExecutorReviewSelect_serial.log`
- 说明：
  - `UnrealEditor-Cmd` 并行运行会出现 `UnrealEditorServer` 噪音日志（连接中断）；已改为串行重跑并以串行日志为准。
  - F10 仅做确认前校验与 `ConfirmRequest` 桥接/JSON 输出，仍为 `simulate_dry_run`，未接入真实资产写入。

## 7. 结论（用户 UE 手测）

- UE 手测结果：`Pass`
- 用户验收结论（摘要）：
  - 所有合法命令仅输出 `Display/Warning`，无 `Error`；阻断场景 `Warning` 符合预期。
  - 所有 `PrepareConfirm` 摘要均完整包含 `request_id/apply_request_id/review_request_id/selection_digest/user_confirmed/ready_to_execute/error_code/reason` 且 `validation=ok`。
  - 正常通过场景命中 `user_confirmed=true`、`ready_to_execute=true`、`error_code=-`、`reason=confirm_request_ready`，并保留 `ScanLevelMeshRisks + actor + camera_focus + actor_path` 行级定位字段。
  - 未确认场景命中 `ready_to_execute=false`、`E4005`、`user_not_confirmed`。
  - digest 篡改场景命中 `ready_to_execute=false`、`E4202`、`selection_digest_mismatch`。
  - 阻断型 `ApplyRequest` 场景命中 `ready_to_execute=false`、`E4203`、`apply_request_not_ready_blocked_rows_present`。
  - `PrepareConfirmJson` 输出 JSON 且包含 `request_id/apply_request_id/review_request_id/selection_digest/user_confirmed/ready_to_execute/error_code/reason/summary/items/blocked/skip_reason/locate_strategy`。
