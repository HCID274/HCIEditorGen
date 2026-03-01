# StageG-SliceG2 测试记录

- 日期：`2026-02-23`
- 切片：`Stage G - Slice G2`
- 目标：`StageGExecuteIntent（validate-only 入口意图包）完整性校验 + Stage G 写权限二次确认 -> StageGWriteEnableRequest（写权限启用请求，dry-run）桥接`
- 范围说明：仍为 dry-run，不触发真实资产写入；本切片只验证 Stage G 入口后“扳手发放”前的二次确认门禁与链路完整性。

## 1. 本地实现与验证（助手）

- 已完成：
  - Runtime 新增 `StageGWriteEnableRequest` 契约 / 桥接 / JSON 序列化器。
  - Editor 新增命令：
    - `HCI.AgentExecutePlanReviewPrepareStageGWriteEnableRequest`
    - `HCI.AgentExecutePlanReviewPrepareStageGWriteEnableRequestJson`
  - 支持 `write_enable_confirmed=0|1` 二次确认门禁与 `tamper=none|digest|intent|handoff|ready` 篡改/阻断验证。
- 编译：`Build.bat HCIEditorGenEditor Win64 Development ...` ✅ 通过
- 自动化：
  - `HCI.Editor.AgentExecutorStageGWriteEnableRequest` ✅ `4/4`
  - `HCI.Editor.AgentExecutorStageGExecuteIntent`（G1 回归）✅ `4/4`
- 证据日志：
  - `Saved/Logs/Automation_G2_StageGWriteEnableRequest.log`
  - `Saved/Logs/Automation_G2_StageGExecuteIntentRegression.log`

## 2. UE 手测步骤（用户执行）

### 2.1 先生成可通过链路（到 G1）

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

### 2.2 G2 正常桥接与 JSON

11. `HCI.AgentExecutePlanReviewPrepareStageGWriteEnableRequest 1 none`
12. `HCI.AgentExecutePlanReviewPrepareStageGWriteEnableRequestJson 1 none`

### 2.3 G2 篡改/阻断校验

13. `HCI.AgentExecutePlanReviewPrepareStageGWriteEnableRequest 0 none`
14. `HCI.AgentExecutePlanReviewPrepareStageGWriteEnableRequest 1 digest`
15. `HCI.AgentExecutePlanReviewPrepareStageGWriteEnableRequest 1 intent`
16. `HCI.AgentExecutePlanReviewPrepareStageGWriteEnableRequest 1 handoff`
17. `HCI.AgentExecutePlanReviewPrepareStageGWriteEnableRequest 1 ready`

## 3. 预期结果（Pass 判定标准）

1. 合法命令无 `Error`
- 阻断场景出现 `Warning` 可接受。

2. `PrepareStageGWriteEnableRequest` 摘要字段完整
- `[HCIAbilityKit][AgentExecutorStageGWriteEnableRequest] summary ...`
- 必含：
  - `request_id/stage_g_execute_intent_id`
  - `sim_handoff_envelope_id/sim_archive_bundle_id/sim_final_report_id/sim_execute_receipt_id`
  - `execute_ticket_id/confirm_request_id/apply_request_id/review_request_id`
  - `selection_digest/archive_digest/handoff_digest/execute_intent_digest/stage_g_write_enable_digest`
  - `execute_target/handoff_target`
  - `user_confirmed/write_enable_confirmed`
  - `terminal_status/archive_status/handoff_status/stage_g_status/stage_g_write_status`
  - `write_enabled/ready_for_stage_g_entry/ready_for_stage_g_execute`
  - `error_code/reason`
  - `validation=ok`

3. 正常场景命中（`1 none`）
- `write_enable_confirmed=true`
- `ready_for_stage_g_execute=true`
- `write_enabled=true`
- `stage_g_write_status=ready`
- `error_code=-`
- `reason=stage_g_write_enable_request_ready`

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
- `ready` -> `E4210 / stage_g_execute_intent_not_ready`

6. `PrepareStageGWriteEnableRequestJson` 输出 JSON 且包含
- `"stage_g_execute_intent_id"`
- `"stage_g_write_enable_digest"`
- `"stage_g_write_status"`
- `"write_enable_confirmed"`
- `"ready_for_stage_g_execute"`
- `"write_enabled"`
- `"summary"`, `"items"`, `"blocked"`, `"skip_reason"`, `"locate_strategy"`

## 4. 实际结果

- 用户 UE 手测结果：`Pass（先发现漏洞，修复后正式通过）`
- 证据要点（用户反馈）：
  - 正常场景（`1 none`）命中 `write_enabled=true`、`ready_for_stage_g_execute=true`、`reason=stage_g_write_enable_request_ready`，确认 Stage G 写权限启用逻辑成功翻转。
  - 阻断场景正确命中：
    - `0 none` -> `E4005 / stage_g_write_enable_not_confirmed`
    - `ready` -> `E4210 / stage_g_execute_intent_not_ready`
    - `digest` -> `E4202 / selection_digest_mismatch`
    - `handoff` -> `E4202 / sim_handoff_envelope_id_mismatch`
  - 用户发现并验证修复关键漏洞：`intent` 篡改在修复前未被拦截；修复后命中 `E4202 / stage_g_execute_intent_id_mismatch`，并且 `stage_g_write_status=blocked`、`write_enabled=false`。
  - 正常路径回归无破坏：修复后再次验证 `1 none` 仍保持 `write_enabled=true` 与 `ready_for_stage_g_execute=true`。
- 结论：`Pass`
