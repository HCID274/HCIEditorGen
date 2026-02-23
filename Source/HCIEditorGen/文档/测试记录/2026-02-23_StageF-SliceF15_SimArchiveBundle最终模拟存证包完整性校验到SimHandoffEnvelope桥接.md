# 2026-02-23 StageF-SliceF15 Executor 模拟存证包到 Stage G 输入信封桥接

## 1. 目标

- 验证 `SimArchiveBundle` -> `SimHandoffEnvelope` 桥接命令在 UE 控制台可用。
- 验证 F15 新增校验：`E4208 / sim_archive_bundle_not_ready`。
- 验证 `SimHandoffEnvelope` JSON 契约字段完整，并保留行级定位证据（`actor_path/camera_focus`）。

## 2. 前置条件

- 已完成并通过 `StageF-SliceF14`，可生成最新 `SimArchiveBundle` 预览状态。
- 在 UE 编辑器控制台执行（非 `UnrealEditor-Cmd`）。

## 3. UE 手测步骤

1. 生成可通过链路：
   - `HCIAbilityKit.AgentExecutePlanReviewDemo ok_level_risk`
   - `HCIAbilityKit.AgentExecutePlanReviewSelect 0`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareApply`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareConfirm 1 none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareExecuteTicket none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimExecuteReceipt none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimFinalReport none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimArchiveBundle none`
2. 正常桥接（应通过）：
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimHandoffEnvelope none`
3. 输出 JSON：
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimHandoffEnvelopeJson none`
4. 篡改/阻断校验：
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimHandoffEnvelope digest`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimHandoffEnvelope confirm`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimHandoffEnvelope archive`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimHandoffEnvelope ready`
5. 未确认链路（应拦截 `E4005`）：
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareConfirm 0 none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareExecuteTicket none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimExecuteReceipt none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimFinalReport none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimArchiveBundle none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimHandoffEnvelope none`

## 4. 预期结果（Pass 标准）

1. 合法命令无 `Error`（阻断场景 `Warning` 可接受）。
2. `PrepareSimHandoffEnvelope` 摘要包含：
   - `request_id/sim_archive_bundle_id/sim_final_report_id/sim_execute_receipt_id/execute_ticket_id/confirm_request_id/apply_request_id/review_request_id/selection_digest/archive_digest/handoff_digest`
   - `handoff_target/terminal_status/archive_status/handoff_status`
   - `user_confirmed/ready_to_simulate_execute/simulated_dispatch_accepted/simulation_completed/archive_ready/handoff_ready`
   - `error_code/reason/validation=ok`
3. 正常场景命中：
   - `handoff_ready=true`
   - `handoff_status=ready`
   - `error_code=-`
   - `reason=simulate_handoff_envelope_ready`
4. 行日志保留定位字段：
   - `ScanLevelMeshRisks + actor + camera_focus + actor_path`
5. 阻断场景命中：
   - `digest -> E4202 / selection_digest_mismatch`
   - `confirm -> E4202 / confirm_request_id_mismatch`
   - `archive -> E4208 / sim_archive_bundle_not_ready`
   - `ready -> E4205 / execute_ticket_not_ready`
   - 未确认链路 -> `E4005 / user_not_confirmed`
6. `PrepareSimHandoffEnvelopeJson` 输出 JSON 包含：
   - `request_id/sim_archive_bundle_id/sim_final_report_id/sim_execute_receipt_id/execute_ticket_id/confirm_request_id/apply_request_id/review_request_id/selection_digest/archive_digest/handoff_digest`
   - `handoff_target/transaction_mode/termination_policy/terminal_status/archive_status/handoff_status`
   - `user_confirmed/ready_to_simulate_execute/simulated_dispatch_accepted/simulation_completed/archive_ready/handoff_ready/error_code/reason`
   - `summary/items/blocked/skip_reason/locate_strategy`

## 5. 本地实现验证（助手）

- 编译：`Build.bat HCIEditorGenEditor Win64 Development ...` 通过。
- 自动化：`HCIAbilityKit.Editor.AgentExecutorSimHandoffEnvelope`（4/4）通过。
- 证据日志：`Saved/Logs/Automation_F15_AgentExecutorSimHandoffEnvelope_serial.log`。

## 6. 结果

- 目标：通过。`SimArchiveBundle -> SimHandoffEnvelope` 桥接命令、F15 新增 `E4208` 校验与 JSON 契约字段完整性均在 UE 控制台验证通过。
- 步骤：用户按预设步骤执行完整链路（`Review -> Select -> Apply -> Confirm -> ExecuteTicket -> SimExecuteReceipt -> SimFinalReport -> SimArchiveBundle -> SimHandoffEnvelope`），并执行 `digest/confirm/archive/ready` 篡改与“未确认链路”阻断测试。
- 结果：
  - 正常场景命中：`handoff_ready=true`、`handoff_status=ready`、`error_code=-`、`reason=simulate_handoff_envelope_ready`。
  - 成功形成 8 级 ID 审计链：`review -> apply -> confirm -> ticket -> receipt -> final -> archive -> handoff`。
  - 三重指纹同时存在：`selection_digest / archive_digest / handoff_digest`。
  - 阻断场景命中：
    - `digest -> E4202 / selection_digest_mismatch`
    - `confirm -> E4202 / confirm_request_id_mismatch`
    - `archive -> E4208 / sim_archive_bundle_not_ready`
    - `ready -> E4205 / execute_ticket_not_ready`
    - 未确认链路 -> `E4005 / user_not_confirmed`
  - `PrepareSimHandoffEnvelopeJson` 输出 JSON 字段完整，包含 `handoff_target=stage_g_execute`、`transaction_mode`、`termination_policy`、`summary/items` 与定位字段。
- 证据：
  - UE 控制台日志（用户手测）：`HCIAbilityKit.AgentExecutePlanReviewPrepareSimHandoffEnvelope*`
  - 自动化日志（助手本地）：`Saved/Logs/Automation_F15_AgentExecutorSimHandoffEnvelope_serial.log`
- 结论：`Pass`（用户 UE 手测通过）。Stage F（F1~F15）dry-run 链路收官，可作为 Stage G 真执行链路的稳定输入基线。
