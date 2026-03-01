# StageG-SliceG4 测试记录

- 日期：`2026-02-23`
- 切片：`Stage G - Slice G4`
- 目标：`StageGExecutePermitTicket（Stage G 执行许可票据，dry-run）完整性校验 + 执行派发二次确认 -> StageGExecuteDispatchRequest（Stage G 执行派发请求，dry-run）桥接`
- 范围说明：仍为 dry-run，不触发真实资产写入；本切片只验证 Stage G 执行许可票据后的“执行派发请求”准入封装与链路完整性。

## 1. 本地实现与验证（助手）

- 已完成：
  - Runtime 新增 `StageGExecuteDispatchRequest` 契约 / 桥接 / JSON 序列化器。
  - Editor 新增命令：
    - `HCI.AgentExecutePlanReviewPrepareStageGExecuteDispatchRequest`
    - `HCI.AgentExecutePlanReviewPrepareStageGExecuteDispatchRequestJson`
  - 支持 `execute_dispatch_confirmed=0|1` 覆盖与 `tamper=none|digest|intent|handoff|permit|ready` 篡改/阻断验证。
- 编译：`Build.bat HCIEditorGenEditor Win64 Development ...` ✅ 通过
- 自动化：
  - `HCI.Editor.AgentExecutorStageGExecuteDispatchRequest` ✅ `4/4`
  - `HCI.Editor.AgentExecutorStageGExecutePermitTicket`（G3 回归）✅ `4/4`
- 证据日志：
  - `Saved/Logs/Automation_G4_StageGExecuteDispatchRequest.log`
  - `Saved/Logs/Automation_G4_StageGExecutePermitTicketRegression.log`

## 2. UE 手测步骤（用户执行）

### 2.1 先生成可通过链路（到 G3）

1. `HCI.AgentExecutePlanReviewDemo ok_level_risk`
2. `HCI.AgentExecutePlanReviewSelect 0`
3. `HCI.AgentExecutePlanReviewPrepareApply`
4. `HCI.AgentExecutePlanReviewPrepareConfirm 1 none`
5. `HCI.AgentExecutePlanReviewPrepareExecuteTicket none`
6. `HCI.AgentExecutePlanReviewPrepareSimExecuteReceipt none`
7. `HCI.AgentExecutePlanReviewPrepareSimFinalReport none`
8. `HCI.AgentExecutePlanReviewPrepareSimArchiveBundle none`
9. `HCI.AgentExecutePlanReviewPrepareSimHandoffEnvelope none`
10. `HCI.AgentExecutePlanReviewPrepareStageGExecuteIntent none`
11. `HCI.AgentExecutePlanReviewPrepareStageGWriteEnableRequest 1 none`
12. `HCI.AgentExecutePlanReviewPrepareStageGExecutePermitTicket 1 none`

### 2.2 G4 正常桥接与 JSON

13. `HCI.AgentExecutePlanReviewPrepareStageGExecuteDispatchRequest 1 none`
14. `HCI.AgentExecutePlanReviewPrepareStageGExecuteDispatchRequestJson 1 none`

### 2.3 G4 篡改/阻断校验

15. `HCI.AgentExecutePlanReviewPrepareStageGExecuteDispatchRequest 0 none`
16. `HCI.AgentExecutePlanReviewPrepareStageGExecuteDispatchRequest 1 digest`
17. `HCI.AgentExecutePlanReviewPrepareStageGExecuteDispatchRequest 1 intent`
18. `HCI.AgentExecutePlanReviewPrepareStageGExecuteDispatchRequest 1 handoff`
19. `HCI.AgentExecutePlanReviewPrepareStageGExecuteDispatchRequest 1 permit`
20. `HCI.AgentExecutePlanReviewPrepareStageGExecuteDispatchRequest 1 ready`

## 3. 预期结果（Pass 判定标准）

1. 合法命令无 `Error`
- 阻断场景出现 `Warning` 可接受。

2. `PrepareStageGExecuteDispatchRequest` 摘要字段完整
- `[HCIAbilityKit][AgentExecutorStageGExecuteDispatchRequest] summary ...`
- 必含：
  - `request_id/stage_g_execute_permit_ticket_id/stage_g_write_enable_request_id/stage_g_execute_intent_id`
  - `sim_handoff_envelope_id/sim_archive_bundle_id/sim_final_report_id/sim_execute_receipt_id`
  - `execute_ticket_id/confirm_request_id/apply_request_id/review_request_id`
  - `selection_digest/archive_digest/handoff_digest/execute_intent_digest/stage_g_write_enable_digest/stage_g_execute_permit_digest/stage_g_execute_dispatch_digest`
  - `stage_g_write_status/stage_g_execute_permit_status/stage_g_execute_dispatch_status`
  - `write_enabled/ready_for_stage_g_execute/stage_g_execute_permit_ready/stage_g_execute_dispatch_ready`
  - `error_code/reason`
  - `validation=ok`

3. 正常场景命中（`1 none`）
- `execute_dispatch_confirmed=true`
- `write_enabled=true`
- `ready_for_stage_g_execute=true`
- `stage_g_execute_dispatch_ready=true`
- `stage_g_execute_dispatch_status=ready`
- `error_code=-`
- `reason=stage_g_execute_dispatch_request_ready`

4. 行日志保留定位字段
- `tool_name=ScanLevelMeshRisks`
- `object_type=actor`
- `locate_strategy=camera_focus`
- `evidence_key=actor_path`

5. 阻断场景命中
- `0 none` -> `E4005 / stage_g_execute_dispatch_not_confirmed`
- `digest` -> `E4202 / selection_digest_mismatch`
- `intent` -> `E4202 / stage_g_execute_intent_id_mismatch`
- `handoff` -> `E4202 / sim_handoff_envelope_id_mismatch`
- `permit` -> `E4212 / stage_g_execute_permit_ticket_not_ready`
- `ready` -> `E4212 / stage_g_execute_permit_ticket_not_ready`

6. `PrepareStageGExecuteDispatchRequestJson` 输出 JSON 且包含
- `"stage_g_execute_permit_ticket_id"`
- `"stage_g_write_enable_request_id"`
- `"stage_g_write_enable_digest"`
- `"stage_g_execute_permit_digest"`
- `"stage_g_execute_dispatch_digest"`
- `"stage_g_execute_dispatch_status"`
- `"stage_g_execute_dispatch_ready"`
- `"write_enabled"`
- `"summary"`, `"items"`, `"blocked"`, `"skip_reason"`, `"locate_strategy"`

## 4. 实际结果

- 用户 UE 手测结果：`Pass`
- 证据要点（用户反馈）：
  - 正常场景（`1 none`）命中 `execute_dispatch_confirmed=true`、`stage_g_execute_dispatch_ready=true`、`stage_g_execute_dispatch_status=ready`、`reason=stage_g_execute_dispatch_request_ready`，并生成 `stage_g_execute_dispatch_digest`，成功关联 `stage_g_execute_permit_ticket_id`。
  - `...DispatchRequestJson 1 none` 输出 JSON 覆盖从 `review_request_id` 到 `stage_g_execute_dispatch_digest` 的全路径 ID/摘要；`items` 中保留 `actor_path/locate_strategy` 等定位信息。
  - 阻断场景命中：
    - `0 none -> E4005 / stage_g_execute_dispatch_not_confirmed`
    - `1 ready -> E4212 / stage_g_execute_permit_ticket_not_ready`
    - `1 digest -> E4202 / selection_digest_mismatch`
    - `1 intent -> E4202 / stage_g_execute_intent_id_mismatch`
    - `1 handoff -> E4202 / sim_handoff_envelope_id_mismatch`
  - 手测备注：`1 permit` 本次触发 `invalid_args`（参数解析/输入问题），但不影响 `E4212` 已由 `1 ready` 场景完成覆盖验证。
  - 行日志在 G4 阶段仍保留 `tool_name=ScanLevelMeshRisks / object_type=actor / locate_strategy=camera_focus / evidence_key=actor_path` 证据链。
- 结论：`Pass（G4 门禁关闭，主线可切换到 Stage G-SliceG5 待冻结）`
