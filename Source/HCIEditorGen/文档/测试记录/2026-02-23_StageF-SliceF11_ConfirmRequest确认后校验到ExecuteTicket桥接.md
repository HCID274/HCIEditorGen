# StageF-SliceF11 测试记录

- 日期：`2026-02-23`
- 切片：`Stage F - Slice F11`
- 目标：`ConfirmRequest` 确认后校验 -> `ExecuteTicket` 执行票据桥接（仍为 `simulate_dry_run`）

## 1. 本地实现与验证（助手完成）

- 新增 Runtime：
  - `HCIAbilityKitAgentExecuteTicket.h`
  - `HCIAbilityKitAgentExecutorExecuteTicketBridge.*`
  - `HCIAbilityKitAgentExecuteTicketJsonSerializer.*`
- 新增 Editor 命令：
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareExecuteTicket [tamper=none|digest|apply|review]`
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareExecuteTicketJson [tamper=none|digest|apply|review]`
- 编译：`Build.bat HCIEditorGenEditor Win64 Development ...` 通过
- 自动化：
  - `HCIAbilityKit.Editor.AgentExecutorExecuteTicket`（4/4）通过
  - `HCIAbilityKit.Editor.AgentExecutorApplyConfirm`（回归）通过
  - `HCIAbilityKit.Editor.AgentExecutorApply`（回归）通过
  - `HCIAbilityKit.Editor.AgentExecutorReviewSelect`（回归）通过

## 2. UE 手测步骤（用户执行）

1. 生成可通过链路（Review -> Select -> Apply -> Confirm）
   - `HCIAbilityKit.AgentExecutePlanReviewDemo ok_level_risk`
   - `HCIAbilityKit.AgentExecutePlanReviewSelect 0`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareApply`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareConfirm 1 none`
2. 正常桥接（应通过）
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareExecuteTicket none`
3. 输出 ExecuteTicket JSON
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareExecuteTicketJson none`
4. digest 篡改（应拦截 `E4202`）
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareExecuteTicket digest`
5. apply_id 篡改（应拦截 `E4202`）
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareExecuteTicket apply`
6. review_id 篡改（应拦截 `E4202`）
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareExecuteTicket review`
7. 未确认链路（应拦截 `E4005`）
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareConfirm 0 none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareExecuteTicket none`

## 3. Pass 判定标准

1. 上述合法命令无 `Error`
   - 阻断场景出现 `Warning` 属预期
2. `PrepareExecuteTicket` 摘要日志包含：
   - `request_id`
   - `confirm_request_id`
   - `apply_request_id`
   - `review_request_id`
   - `selection_digest`
   - `user_confirmed`
   - `ready_to_simulate_execute`
   - `error_code`
   - `reason`
   - `validation=ok`
3. 正常通过场景命中：
   - `user_confirmed=true`
   - `ready_to_simulate_execute=true`
   - `error_code=-`
   - `reason=execute_ticket_ready`
4. 正常场景行日志保留定位字段：
   - `tool_name=ScanLevelMeshRisks`
   - `object_type=actor`
   - `locate_strategy=camera_focus`
   - `evidence_key=actor_path`
5. `digest` 篡改场景命中：
   - `ready_to_simulate_execute=false`
   - `error_code=E4202`
   - `reason=selection_digest_mismatch`
6. `apply` 篡改场景命中：
   - `ready_to_simulate_execute=false`
   - `error_code=E4202`
   - `reason=apply_request_id_mismatch`
7. `review` 篡改场景命中：
   - `ready_to_simulate_execute=false`
   - `error_code=E4202`
   - `reason=review_request_id_mismatch`
8. 未确认场景命中：
   - `ready_to_simulate_execute=false`
   - `error_code=E4005`
   - `reason=user_not_confirmed`
9. `PrepareExecuteTicketJson` 输出 JSON 且包含：
   - `"request_id"`, `"confirm_request_id"`, `"apply_request_id"`, `"review_request_id"`
   - `"selection_digest"`, `"transaction_mode"`, `"termination_policy"`
   - `"user_confirmed"`, `"ready_to_simulate_execute"`, `"error_code"`, `"reason"`
   - `"summary"`, `"items"`, `"blocked"`, `"skip_reason"`, `"locate_strategy"`

## 4. 结果（用户 UE 手测）

- 结果：`Pass`
- 证据（用户日志结论摘要）：
  - 所有合法命令仅输出 `Display/Warning`，无 `Error`；
  - `PrepareExecuteTicket` 摘要完整命中 `request_id/confirm_request_id/apply_request_id/review_request_id/selection_digest/user_confirmed/ready_to_simulate_execute/error_code/reason validation=ok`；
  - 正常场景命中 `ready_to_simulate_execute=true error_code=- reason=execute_ticket_ready`；
  - 行日志保留 `ScanLevelMeshRisks + actor + camera_focus + actor_path` 定位字段；
  - `digest/apply/review` 篡改分别命中 `E4202/selection_digest_mismatch`、`E4202/apply_request_id_mismatch`、`E4202/review_request_id_mismatch`；
  - 未确认场景命中 `E4005/user_not_confirmed`；
  - `PrepareExecuteTicketJson` 输出 `ExecuteTicket` JSON 核心字段完整。
- 结论：`Stage F-SliceF11 Pass，主线可切换到 Stage F-SliceF12（待定义/待冻结）`
