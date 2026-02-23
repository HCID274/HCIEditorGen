# StageF-SliceF14 测试记录

- 日期：`2026-02-23`
- 切片：`Stage F - Slice F14`
- 目标：`SimFinalReport` 最终模拟执行报告完整性校验 -> `SimArchiveBundle` 最终模拟存证包桥接（仍为 `simulate_dry_run`）

## 1. 本地实现与验证（助手完成）

- 新增 Runtime：
  - `HCIAbilityKitAgentSimulateExecuteArchiveBundle.h`
  - `HCIAbilityKitAgentExecutorSimulateExecuteArchiveBundleBridge.*`
  - `HCIAbilityKitAgentSimulateExecuteArchiveBundleJsonSerializer.*`
- 新增 Editor 命令：
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimArchiveBundle [tamper=none|digest|apply|review|confirm|receipt|final|ready]`
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimArchiveBundleJson [tamper=none|digest|apply|review|confirm|receipt|final|ready]`
- 新增自动化测试：
  - `HCIAbilityKit.Editor.AgentExecutorSimArchiveBundle.*`（成功 / `E4207` / `E4202` / JSON 字段）
- 编译：`Build.bat HCIEditorGenEditor Win64 Development ...` 通过
- 自动化：
  - `HCIAbilityKit.Editor.AgentExecutorSimArchiveBundle`：`4/4` 通过（见 `Saved/Logs/Automation_F14_AgentExecutorSimArchiveBundle_serial.log`）
  - `HCIAbilityKit.Editor.AgentExecutorSimFinalReport`：`4/4` 回归通过（见 `Saved/Logs/Automation_F14_AgentExecutorSimFinalReport_serial.log`）
  - 说明：并行启动 UE-Cmd 时出现 `UnrealEditorServer` 网络噪音日志，但 `abslog` 已稳定产出结果行，不影响本切片进入 `UE 手测门禁`。

## 2. UE 手测步骤（用户执行）

1. 生成可通过链路（Review -> Select -> Apply -> Confirm -> ExecuteTicket -> SimExecuteReceipt -> SimFinalReport）
   - `HCIAbilityKit.AgentExecutePlanReviewDemo ok_level_risk`
   - `HCIAbilityKit.AgentExecutePlanReviewSelect 0`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareApply`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareConfirm 1 none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareExecuteTicket none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimExecuteReceipt none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimFinalReport none`
2. 正常桥接（应通过）
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimArchiveBundle none`
3. 输出 JSON
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimArchiveBundleJson none`
4. digest 篡改（应拦截 `E4202`）
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimArchiveBundle digest`
5. confirm_id 篡改（应拦截 `E4202`）
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimArchiveBundle confirm`
6. 回执未接受（应拦截 `E4206`）
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimArchiveBundle receipt`
7. FinalReport 未完成（应拦截 `E4207`）
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimArchiveBundle final`
8. 强制 not-ready（应拦截 `E4205`）
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimArchiveBundle ready`
9. 未确认链路（应拦截 `E4005`）
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareConfirm 0 none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareExecuteTicket none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimExecuteReceipt none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimFinalReport none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimArchiveBundle none`

## 3. Pass 判定标准

1. 上述合法命令无 `Error`
   - 阻断场景出现 `Warning` 属预期
2. `PrepareSimArchiveBundle` 摘要日志包含：
   - `request_id/sim_final_report_id/sim_execute_receipt_id/execute_ticket_id/confirm_request_id/apply_request_id/review_request_id/selection_digest/archive_digest`
   - `user_confirmed/ready_to_simulate_execute/simulated_dispatch_accepted/simulation_completed/terminal_status/archive_status/archive_ready/error_code/reason`
   - `validation=ok`
3. 正常场景命中：
   - `archive_ready=true`
   - `archive_status=ready`
   - `error_code=-`
   - `reason=simulate_archive_bundle_ready`
4. 正常场景行日志保留定位字段：
   - `tool_name=ScanLevelMeshRisks`
   - `object_type=actor`
   - `locate_strategy=camera_focus`
   - `evidence_key=actor_path`
5. 阻断场景命中：
   - `digest` -> `E4202 / selection_digest_mismatch`
   - `confirm` -> `E4202 / confirm_request_id_mismatch`
   - `receipt` -> `E4206 / simulate_execute_receipt_not_accepted`
   - `final` -> `E4207 / simulate_final_report_not_completed`
   - `ready` -> `E4205 / execute_ticket_not_ready`
   - 未确认链路 -> `E4005 / user_not_confirmed`
6. `PrepareSimArchiveBundleJson` 输出 JSON 且包含：
   - `request_id/sim_final_report_id/sim_execute_receipt_id/execute_ticket_id/confirm_request_id/apply_request_id/review_request_id/selection_digest/archive_digest`
   - `transaction_mode/termination_policy/terminal_status/archive_status`
   - `user_confirmed/ready_to_simulate_execute/simulated_dispatch_accepted/simulation_completed/archive_ready/error_code/reason`
   - `summary/items/blocked/skip_reason/locate_strategy`

## 4. 结果（用户 UE 手测）

- 结果：`Pass`
- 证据（用户日志结论摘要）：
  - 正常场景 `HCIAbilityKit.AgentExecutePlanReviewPrepareSimArchiveBundle none` 命中 `archive_ready=true archive_status=ready reason=simulate_archive_bundle_ready`，并输出 `archive_digest=crc32_1DA50398` 与完整 7 级 ID 链；
  - 阻断场景命中：
    - `digest` -> `E4202 / selection_digest_mismatch`
    - `confirm` -> `E4202 / confirm_request_id_mismatch`
    - `receipt` -> `E4206 / simulate_execute_receipt_not_accepted`
    - `final` -> `E4207 / simulate_final_report_not_completed`
    - 未确认链路 -> `E4005 / user_not_confirmed`
  - `PrepareSimArchiveBundleJson` 输出 JSON，包含 `summary`（total/selected/modifiable/read_only 等）以及 `archive_digest/archive_status` 等 F14 核心字段；
  - 行日志保留 `actor_path` 与 `locate_strategy=camera_focus` 等定位证据链。
- 结论：`Stage F-SliceF14 Pass，主线可切换到 Stage F-SliceF15（待定义/待冻结）`
