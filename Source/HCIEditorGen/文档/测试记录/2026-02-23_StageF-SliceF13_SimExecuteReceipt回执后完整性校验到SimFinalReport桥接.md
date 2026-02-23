# StageF-SliceF13 测试记录（待验证）

- 日期：`2026-02-23`
- 切片：`Stage F - Slice F13`
- 目标：`SimExecuteReceipt` 回执后完整性校验 -> `SimFinalReport` 最终模拟执行报告桥接（仍为 `simulate_dry_run`）

## 1. 本地实现与验证（助手完成）

- 新增 Runtime：
  - `HCIAbilityKitAgentSimulateExecuteFinalReport.h`
  - `HCIAbilityKitAgentExecutorSimulateExecuteFinalReportBridge.*`
  - `HCIAbilityKitAgentSimulateExecuteFinalReportJsonSerializer.*`
- 新增 Editor 命令：
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimFinalReport [tamper=none|digest|apply|review|confirm|receipt|ready]`
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimFinalReportJson [tamper=none|digest|apply|review|confirm|receipt|ready]`
- 新增自动化测试：
  - `HCIAbilityKit.Editor.AgentExecutorSimFinalReport.*`（成功 / `E4206` / `E4202` / JSON 字段）
- 编译：`Build.bat HCIEditorGenEditor Win64 Development ...` 通过
- 自动化（说明）：
  - 已补齐 F13 自动化测试代码，但本机 `UE-Cmd` 在执行自动化时再次触发退出期 `ScopeLock` 断言噪音，导致 `abslog` 未稳定产出测试结果行；
  - 已记录为环境问题（非业务代码编译错误），本切片先进入 `UE 手测门禁`。

## 2. UE 手测步骤（用户执行）

1. 生成可通过链路（Review -> Select -> Apply -> Confirm -> ExecuteTicket -> SimExecuteReceipt）
   - `HCIAbilityKit.AgentExecutePlanReviewDemo ok_level_risk`
   - `HCIAbilityKit.AgentExecutePlanReviewSelect 0`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareApply`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareConfirm 1 none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareExecuteTicket none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimExecuteReceipt none`
2. 正常桥接（应通过）
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimFinalReport none`
3. 输出 JSON
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimFinalReportJson none`
4. digest 篡改（应拦截 `E4202`）
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimFinalReport digest`
5. confirm_id 篡改（应拦截 `E4202`）
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimFinalReport confirm`
6. 强制回执未接受（应拦截 `E4206`）
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimFinalReport receipt`
7. 强制 not-ready（应拦截 `E4205`）
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimFinalReport ready`
8. 未确认链路（应拦截 `E4005`）
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareConfirm 0 none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareExecuteTicket none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimExecuteReceipt none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimFinalReport none`

## 3. Pass 判定标准

1. 上述合法命令无 `Error`
   - 阻断场景出现 `Warning` 属预期
2. `PrepareSimFinalReport` 摘要日志包含：
   - `request_id`
   - `sim_execute_receipt_id`
   - `execute_ticket_id`
   - `confirm_request_id`
   - `apply_request_id`
   - `review_request_id`
   - `selection_digest`
   - `user_confirmed`
   - `ready_to_simulate_execute`
   - `simulated_dispatch_accepted`
   - `simulation_completed`
   - `terminal_status`
   - `error_code`
   - `reason`
   - `validation=ok`
3. 正常通过场景命中：
   - `simulated_dispatch_accepted=true`
   - `simulation_completed=true`
   - `terminal_status=completed`
   - `error_code=-`
   - `reason=simulate_execute_final_report_ready`
4. 正常场景行日志保留定位字段：
   - `tool_name=ScanLevelMeshRisks`
   - `object_type=actor`
   - `locate_strategy=camera_focus`
   - `evidence_key=actor_path`
5. `digest` 篡改场景命中：
   - `simulation_completed=false`
   - `error_code=E4202`
   - `reason=selection_digest_mismatch`
6. `confirm` 篡改场景命中：
   - `simulation_completed=false`
   - `error_code=E4202`
   - `reason=confirm_request_id_mismatch`
7. `receipt` 强制回执未接受场景命中：
   - `simulation_completed=false`
   - `error_code=E4206`
   - `reason=simulate_execute_receipt_not_accepted`
8. `ready` 强制 not-ready 场景命中：
   - `simulation_completed=false`
   - `error_code=E4205`
   - `reason=execute_ticket_not_ready`
9. 未确认链路命中：
   - `simulation_completed=false`
   - `error_code=E4005`
   - `reason=user_not_confirmed`
10. `PrepareSimFinalReportJson` 输出 JSON 且包含：
   - `"request_id"`, `"sim_execute_receipt_id"`, `"execute_ticket_id"`, `"confirm_request_id"`, `"apply_request_id"`, `"review_request_id"`
   - `"selection_digest"`, `"transaction_mode"`, `"termination_policy"`, `"terminal_status"`
   - `"user_confirmed"`, `"ready_to_simulate_execute"`, `"simulated_dispatch_accepted"`, `"simulation_completed"`, `"error_code"`, `"reason"`
   - `"summary"`, `"items"`, `"blocked"`, `"skip_reason"`, `"locate_strategy"`

## 4. 结果（用户 UE 手测）

- 结果：`Pass`
- 证据（用户日志结论摘要）：
  - 正常场景 `HCIAbilityKit.AgentExecutePlanReviewPrepareSimFinalReport none` 命中 `simulation_completed=true terminal_status=completed error_code=- reason=simulate_execute_final_report_ready`；
  - 篡改与阻断场景命中：
    - `digest` -> `E4202 / selection_digest_mismatch`
    - `confirm` -> `E4202 / confirm_request_id_mismatch`
    - `receipt` -> `E4206 / simulate_execute_receipt_not_accepted`
    - `ready` -> `E4205 / execute_ticket_not_ready`
    - 未确认链路 -> `E4005 / user_not_confirmed`
  - `PrepareSimFinalReportJson` 输出 JSON，包含 `transaction_mode/termination_policy/terminal_status/summary/items` 与定位/证据字段；
  - 行日志保留 `ScanLevelMeshRisks + actor + camera_focus + actor_path` 审计定位证据链。
- 结论：`Stage F-SliceF13 Pass，主线可切换到 Stage F-SliceF14（待定义/待冻结）`
