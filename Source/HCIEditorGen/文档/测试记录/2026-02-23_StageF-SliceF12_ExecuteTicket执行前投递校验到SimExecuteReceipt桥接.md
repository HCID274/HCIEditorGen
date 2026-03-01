# StageF-SliceF12 测试记录

- 日期：`2026-02-23`
- 切片：`Stage F - Slice F12`
- 目标：`ExecuteTicket` 执行前投递校验 -> `SimExecuteReceipt` 模拟执行回执桥接（仍为 `simulate_dry_run`）

## 1. 本地实现与验证（助手完成）

- 新增 Runtime：
  - `HCIAbilityKitAgentSimulateExecuteReceipt.h`
  - `HCIAbilityKitAgentExecutorSimulateExecuteReceiptBridge.*`
  - `HCIAbilityKitAgentSimulateExecuteReceiptJsonSerializer.*`
- 新增 Editor 命令：
  - `HCI.AgentExecutePlanReviewPrepareSimExecuteReceipt [tamper=none|digest|apply|review|confirm|ready]`
  - `HCI.AgentExecutePlanReviewPrepareSimExecuteReceiptJson [tamper=none|digest|apply|review|confirm|ready]`
- 编译：`Build.bat HCIEditorGenEditor Win64 Development ...` 通过
- 自动化（UE-Cmd `-TestExit="Automation Test Queue Empty"`，`-abslog` 分离留证）：
  - `HCI.Editor.AgentExecutorSimExecuteReceipt`（4/4）通过
  - `HCI.Editor.AgentExecutorExecuteTicket`（4/4，回归）通过
  - `HCI.Editor.AgentExecutorApplyConfirm`（5/5，回归）通过
  - `HCI.Editor.AgentExecutorApply`（前缀命中 8/8，含 `Apply + ApplyConfirm` 回归）通过
  - `HCI.Editor.AgentExecutorReviewSelect`（3/3，回归）通过
- 自动化日志证据：
  - `Saved/Logs/Automation_F12_AgentExecutorSimExecuteReceipt_serial.log`
  - `Saved/Logs/Automation_F12_AgentExecutorExecuteTicket_serial.log`
  - `Saved/Logs/Automation_F12_AgentExecutorApplyConfirm_serial.log`
  - `Saved/Logs/Automation_F12_AgentExecutorApply_serial.log`
  - `Saved/Logs/Automation_F12_AgentExecutorReviewSelect_serial.log`

## 2. UE 手测步骤（用户执行）

1. 生成可通过链路（Review -> Select -> Apply -> Confirm -> ExecuteTicket）
   - `HCI.AgentExecutePlanReviewDemo ok_level_risk`
   - `HCI.AgentExecutePlanReviewSelect 0`
   - `HCI.AgentExecutePlanReviewPrepareApply`
   - `HCI.AgentExecutePlanReviewPrepareConfirm 1 none`
   - `HCI.AgentExecutePlanReviewPrepareExecuteTicket none`
2. 正常桥接（应通过）
   - `HCI.AgentExecutePlanReviewPrepareSimExecuteReceipt none`
3. 输出 SimExecuteReceipt JSON
   - `HCI.AgentExecutePlanReviewPrepareSimExecuteReceiptJson none`
4. digest 篡改（应拦截 `E4202`）
   - `HCI.AgentExecutePlanReviewPrepareSimExecuteReceipt digest`
5. confirm_id 篡改（应拦截 `E4202`）
   - `HCI.AgentExecutePlanReviewPrepareSimExecuteReceipt confirm`
6. 强制 not-ready（应拦截 `E4205`）
   - `HCI.AgentExecutePlanReviewPrepareSimExecuteReceipt ready`
7. 未确认链路（应拦截 `E4005`）
   - `HCI.AgentExecutePlanReviewPrepareConfirm 0 none`
   - `HCI.AgentExecutePlanReviewPrepareExecuteTicket none`
   - `HCI.AgentExecutePlanReviewPrepareSimExecuteReceipt none`

## 3. Pass 判定标准

1. 上述合法命令无 `Error`
   - 阻断场景出现 `Warning` 属预期
2. `PrepareSimExecuteReceipt` 摘要日志包含：
   - `request_id`
   - `execute_ticket_id`
   - `confirm_request_id`
   - `apply_request_id`
   - `review_request_id`
   - `selection_digest`
   - `user_confirmed`
   - `ready_to_simulate_execute`
   - `simulated_dispatch_accepted`
   - `error_code`
   - `reason`
   - `validation=ok`
3. 正常通过场景命中：
   - `user_confirmed=true`
   - `ready_to_simulate_execute=true`
   - `simulated_dispatch_accepted=true`
   - `error_code=-`
   - `reason=simulate_execute_receipt_ready`
4. 正常场景行日志保留定位字段：
   - `tool_name=ScanLevelMeshRisks`
   - `object_type=actor`
   - `locate_strategy=camera_focus`
   - `evidence_key=actor_path`
5. `digest` 篡改场景命中：
   - `simulated_dispatch_accepted=false`
   - `error_code=E4202`
   - `reason=selection_digest_mismatch`
6. `confirm` 篡改场景命中：
   - `simulated_dispatch_accepted=false`
   - `error_code=E4202`
   - `reason=confirm_request_id_mismatch`
7. `ready` 强制 not-ready 场景命中：
   - `simulated_dispatch_accepted=false`
   - `error_code=E4205`
   - `reason=execute_ticket_not_ready`
8. 未确认场景命中：
   - `simulated_dispatch_accepted=false`
   - `error_code=E4005`
   - `reason=user_not_confirmed`
9. `PrepareSimExecuteReceiptJson` 输出 JSON 且包含：
   - `"request_id"`, `"execute_ticket_id"`, `"confirm_request_id"`, `"apply_request_id"`, `"review_request_id"`
   - `"selection_digest"`, `"transaction_mode"`, `"termination_policy"`
   - `"user_confirmed"`, `"ready_to_simulate_execute"`, `"simulated_dispatch_accepted"`, `"error_code"`, `"reason"`
   - `"summary"`, `"items"`, `"blocked"`, `"skip_reason"`, `"locate_strategy"`

## 4. 结果（用户 UE 手测）

- 结果：`Pass`
- 证据（用户日志结论摘要）：
  - 正常场景 `HCI.AgentExecutePlanReviewPrepareSimExecuteReceipt none` 命中 `simulated_dispatch_accepted=true error_code=- reason=simulate_execute_receipt_ready`；
  - 篡改与阻断场景命中：
    - `digest` -> `E4202 / selection_digest_mismatch`
    - `confirm` -> `E4202 / confirm_request_id_mismatch`
    - `ready` -> `E4205 / execute_ticket_not_ready`
    - 未确认链路 -> `E4005 / user_not_confirmed`
  - `PrepareSimExecuteReceiptJson` 输出 JSON，包含 `transaction_mode/termination_policy/summary/items` 与定位/证据字段；
  - 行日志保留 `ScanLevelMeshRisks + actor + camera_focus + actor_path` 审计定位证据链。
- 结论：`Stage F-SliceF12 Pass，主线可切换到 Stage F-SliceF13（待定义/待冻结）`
