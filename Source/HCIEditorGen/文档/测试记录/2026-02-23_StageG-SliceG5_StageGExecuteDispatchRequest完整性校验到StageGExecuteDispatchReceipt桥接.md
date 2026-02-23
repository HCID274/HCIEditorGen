# StageG-SliceG5 测试记录

- 日期：`2026-02-23`
- 切片：`Stage G - Slice G5`
- 目标：`StageGExecuteDispatchRequest（Stage G 执行派发请求，dry-run）完整性校验 -> StageGExecuteDispatchReceipt（Stage G 执行派发回执，dry-run）桥接`
- 范围说明：仍为 dry-run，不触发真实资产写入；本切片只验证 Stage G 执行派发请求后的“执行派发回执”准入封装与链路完整性。

## 1. 本地实现与验证（助手）

- 已完成：
  - Runtime 新增 `StageGExecuteDispatchReceipt` 契约 / 桥接 / JSON 序列化器。
  - Editor 新增命令：
    - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchReceipt`
    - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchReceiptJson`
  - 支持 `tamper=none|digest|intent|handoff|dispatch|ready` 篡改/阻断验证。
- 编译：`Build.bat HCIEditorGenEditor Win64 Development ...` ✅ 通过
- 自动化：
  - `HCIAbilityKit.Editor.AgentExecutorStageGExecuteDispatchReceipt` ✅ `4/4`
  - `HCIAbilityKit.Editor.AgentExecutorStageGExecuteDispatchRequest`（G4 回归）✅ `4/4`
- 证据日志：
  - `Saved/Logs/Automation_G5_StageGExecuteDispatchReceipt.log`
  - `Saved/Logs/Automation_G5_StageGExecuteDispatchRequestRegression.log`

## 2. UE 手测步骤（用户执行）

### 2.1 先生成可通过链路（到 G4）

1. `HCIAbilityKit.AgentExecutePlanReviewDemo ok_level_risk`
2. `HCIAbilityKit.AgentExecutePlanReviewSelect 0`
3. `HCIAbilityKit.AgentExecutePlanReviewPrepareApply`
4. `HCIAbilityKit.AgentExecutePlanReviewPrepareConfirm 1 none`
5. `HCIAbilityKit.AgentExecutePlanReviewPrepareExecuteTicket none`
6. `HCIAbilityKit.AgentExecutePlanReviewPrepareSimExecuteReceipt none`
7. `HCIAbilityKit.AgentExecutePlanReviewPrepareSimFinalReport none`
8. `HCIAbilityKit.AgentExecutePlanReviewPrepareSimArchiveBundle none`
9. `HCIAbilityKit.AgentExecutePlanReviewPrepareSimHandoffEnvelope none`
10. `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteIntent none`
11. `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGWriteEnableRequest 1 none`
12. `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecutePermitTicket 1 none`
13. `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchRequest 1 none`

### 2.2 G5 正常桥接与 JSON

14. `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchReceipt none`
15. `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchReceiptJson none`

### 2.3 G5 篡改/阻断校验

16. `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchReceipt digest`
17. `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchReceipt intent`
18. `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchReceipt handoff`
19. `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchReceipt dispatch`
20. `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchReceipt ready`

### 2.4 （可选）未确认链路穿透回溯

21. `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchRequest 0 none`
22. `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchReceipt none`

## 3. 预期结果（Pass 判定标准）

1. 合法命令无 `Error`
- 阻断场景出现 `Warning` 可接受。

2. `PrepareStageGExecuteDispatchReceipt` 摘要字段完整
- `[HCIAbilityKit][AgentExecutorStageGExecuteDispatchReceipt] summary ...`
- 必含：
  - `request_id/stage_g_execute_dispatch_request_id/stage_g_execute_permit_ticket_id/stage_g_write_enable_request_id/stage_g_execute_intent_id`
  - `selection_digest/archive_digest/handoff_digest/execute_intent_digest/stage_g_write_enable_digest/stage_g_execute_permit_digest/stage_g_execute_dispatch_digest/stage_g_execute_dispatch_receipt_digest`
  - `stage_g_execute_dispatch_status/stage_g_execute_dispatch_receipt_status`
  - `write_enabled/ready_for_stage_g_execute/stage_g_execute_dispatch_ready/stage_g_execute_dispatch_accepted/stage_g_execute_dispatch_receipt_ready`
  - `error_code/reason`
  - `validation=ok`

3. 正常场景命中（`none`）
- `write_enabled=true`
- `ready_for_stage_g_execute=true`
- `stage_g_execute_dispatch_ready=true`
- `stage_g_execute_dispatch_accepted=true`
- `stage_g_execute_dispatch_receipt_ready=true`
- `stage_g_execute_dispatch_receipt_status=accepted`
- `error_code=-`
- `reason=stage_g_execute_dispatch_receipt_ready`

4. 行日志保留定位字段
- `tool_name=ScanLevelMeshRisks`
- `object_type=actor`
- `locate_strategy=camera_focus`
- `evidence_key=actor_path`

5. 阻断场景命中
- `digest` -> `E4202 / selection_digest_mismatch`
- `intent` -> `E4202 / stage_g_execute_intent_id_mismatch`
- `handoff` -> `E4202 / sim_handoff_envelope_id_mismatch`
- `dispatch` -> `E4202 / stage_g_execute_dispatch_request_id_mismatch`
- `ready` -> `E4213 / stage_g_execute_dispatch_request_not_ready`
- （可选）未确认链路穿透：`E4005 / stage_g_execute_dispatch_not_confirmed`

6. `PrepareStageGExecuteDispatchReceiptJson` 输出 JSON 且包含
- `"stage_g_execute_dispatch_request_id"`
- `"stage_g_execute_dispatch_digest"`
- `"stage_g_execute_dispatch_receipt_digest"`
- `"stage_g_execute_dispatch_status"`
- `"stage_g_execute_dispatch_receipt_status"`
- `"stage_g_execute_dispatch_accepted"`
- `"stage_g_execute_dispatch_receipt_ready"`
- `"summary"`, `"items"`, `"blocked"`, `"skip_reason"`, `"locate_strategy"`

## 4. 实际结果

- 用户 UE 手测结果：`Pass`
- 证据要点：
  - 正常场景（`none`）命中 `stage_g_execute_dispatch_accepted=true`、`stage_g_execute_dispatch_receipt_ready=true`、`stage_g_execute_dispatch_receipt_status=accepted`，并生成 `stage_g_execute_dispatch_receipt_digest`。
  - JSON 输出包含从 `review_request_id` 到 `stage_g_execute_dispatch_receipt_digest` 的全链路 ID/摘要，保留 `summary/items/locate_strategy/evidence_key/actor_path`。
  - 阻断场景命中：`digest -> E4202/selection_digest_mismatch`、`intent -> E4202/stage_g_execute_intent_id_mismatch`、`handoff -> E4202/sim_handoff_envelope_id_mismatch`、`dispatch -> E4202/stage_g_execute_dispatch_request_id_mismatch`、`ready -> E4213/stage_g_execute_dispatch_request_not_ready`。
  - 可选未确认链路穿透命中：`E4005/stage_g_execute_dispatch_not_confirmed`。
  - 行日志在 G5 阶段仍保留 `ScanLevelMeshRisks + actor/camera_focus/actor_path` 审计定位证据链。
- 结论：`Pass（Stage G-SliceG5 门禁关闭，可进入 Stage G-SliceG6 冻结）`
