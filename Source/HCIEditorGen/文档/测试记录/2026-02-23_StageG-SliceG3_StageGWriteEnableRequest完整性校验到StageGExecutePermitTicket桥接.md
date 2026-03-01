# StageG-SliceG3 测试记录

- 日期：`2026-02-23`
- 切片：`Stage G - Slice G3`
- 目标：`StageGWriteEnableRequest（写权限启用请求，dry-run）完整性校验 -> StageGExecutePermitTicket（Stage G 执行许可票据，dry-run）桥接`
- 范围说明：仍为 dry-run，不触发真实资产写入；本切片只验证 Stage G 写权限请求后的“执行许可票据”准入封装与链路完整性。

## 1. 本地实现与验证（助手）

- 已完成：
  - Runtime 新增 `StageGExecutePermitTicket` 契约 / 桥接 / JSON 序列化器。
  - Editor 新增命令：
    - `HCI.AgentExecutePlanReviewPrepareStageGExecutePermitTicket`
    - `HCI.AgentExecutePlanReviewPrepareStageGExecutePermitTicketJson`
  - 支持 `write_enable_confirmed=0|1` 覆盖与 `tamper=none|digest|intent|handoff|write|ready` 篡改/阻断验证。
- 编译：`Build.bat HCIEditorGenEditor Win64 Development ...` ✅ 通过
- 自动化：
  - `HCI.Editor.AgentExecutorStageGExecutePermitTicket` ✅ `4/4`
  - `HCI.Editor.AgentExecutorStageGWriteEnableRequest`（G2 回归）✅ `5/5`
- 证据日志：
  - `Saved/Logs/Automation_G3_StageGExecutePermitTicket.log`
  - `Saved/Logs/Automation_G3_StageGWriteEnableRequestRegression.log`

## 2. UE 手测步骤（用户执行）

### 2.1 先生成可通过链路（到 G2）

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

### 2.2 G3 正常桥接与 JSON

12. `HCI.AgentExecutePlanReviewPrepareStageGExecutePermitTicket 1 none`
13. `HCI.AgentExecutePlanReviewPrepareStageGExecutePermitTicketJson 1 none`

### 2.3 G3 篡改/阻断校验

14. `HCI.AgentExecutePlanReviewPrepareStageGExecutePermitTicket 0 none`
15. `HCI.AgentExecutePlanReviewPrepareStageGExecutePermitTicket 1 digest`
16. `HCI.AgentExecutePlanReviewPrepareStageGExecutePermitTicket 1 intent`
17. `HCI.AgentExecutePlanReviewPrepareStageGExecutePermitTicket 1 handoff`
18. `HCI.AgentExecutePlanReviewPrepareStageGExecutePermitTicket 1 write`
19. `HCI.AgentExecutePlanReviewPrepareStageGExecutePermitTicket 1 ready`

## 3. 预期结果（Pass 判定标准）

1. 合法命令无 `Error`
- 阻断场景出现 `Warning` 可接受。

2. `PrepareStageGExecutePermitTicket` 摘要字段完整
- `[HCIAbilityKit][AgentExecutorStageGExecutePermitTicket] summary ...`
- 必含：
  - `request_id/stage_g_write_enable_request_id/stage_g_execute_intent_id`
  - `sim_handoff_envelope_id/sim_archive_bundle_id/sim_final_report_id/sim_execute_receipt_id`
  - `execute_ticket_id/confirm_request_id/apply_request_id/review_request_id`
  - `selection_digest/archive_digest/handoff_digest/execute_intent_digest/stage_g_write_enable_digest/stage_g_execute_permit_digest`
  - `stage_g_write_status/stage_g_execute_permit_status`
  - `write_enabled/ready_for_stage_g_execute/stage_g_execute_permit_ready`
  - `error_code/reason`
  - `validation=ok`

3. 正常场景命中（`1 none`）
- `write_enable_confirmed=true`
- `write_enabled=true`
- `ready_for_stage_g_execute=true`
- `stage_g_execute_permit_ready=true`
- `stage_g_execute_permit_status=ready`
- `error_code=-`
- `reason=stage_g_execute_permit_ticket_ready`

4. 行日志保留定位字段
- `tool_name=ScanLevelMeshRisks`
- `object_type=actor`
- `locate_strategy=camera_focus`
- `evidence_key=actor_path`

5. 阻断场景命中
- `0 none` -> `E4005 / stage_g_write_enable_not_confirmed`
- `digest` -> `E4202 / selection_digest_mismatch`
- `intent` -> `E4202 / stage_g_execute_intent_id_mismatch`
- `handoff` -> `E4202 / sim_handoff_envelope_id_mismatch`
- `write` -> `E4211 / stage_g_write_enable_request_not_ready`
- `ready` -> `E4211 / stage_g_write_enable_request_not_ready`

6. `PrepareStageGExecutePermitTicketJson` 输出 JSON 且包含
- `"stage_g_write_enable_request_id"`
- `"stage_g_write_enable_digest"`
- `"stage_g_execute_permit_digest"`
- `"stage_g_execute_permit_status"`
- `"stage_g_execute_permit_ready"`
- `"write_enabled"`
- `"summary"`, `"items"`, `"blocked"`, `"skip_reason"`, `"locate_strategy"`

## 4. 实际结果

- 用户 UE 手测结果：`Pass`
- 证据要点（用户反馈）：
  - 正常场景（`1 none`）命中：`stage_g_execute_permit_ready=true`、`stage_g_execute_permit_status=ready`、`ready_for_stage_g_execute=true`、`write_enabled=true`、`error_code=-`、`reason=stage_g_execute_permit_ticket_ready`，并生成 `stage_g_execute_permit_digest`。
  - `PrepareStageGExecutePermitTicketJson` 输出 JSON 结构完整，包含从 `apply_request_id` 到 `stage_g_write_enable_request_id` 的溯源链，以及 `summary/items`、`locate_strategy/evidence_key/actor_path` 等审计定位字段。
  - 阻断场景正确命中：
    - `0 none` -> `E4005 / stage_g_write_enable_not_confirmed`
    - `1 write` -> `E4211 / stage_g_write_enable_request_not_ready`
    - `1 ready` -> `E4211 / stage_g_write_enable_request_not_ready`
    - `1 digest` -> `E4202 / selection_digest_mismatch`
    - `1 intent` -> `E4202 / stage_g_execute_intent_id_mismatch`
    - `1 handoff` -> `E4202 / sim_handoff_envelope_id_mismatch`
  - 行日志持续保留：`tool_name=ScanLevelMeshRisks`、`object_type=actor`、`locate_strategy=camera_focus`、`evidence_key=actor_path`，满足 G3 末端审计追踪要求。
- 结论：`Pass`
