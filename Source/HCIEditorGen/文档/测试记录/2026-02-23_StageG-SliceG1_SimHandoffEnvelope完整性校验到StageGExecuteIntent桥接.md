# StageG-SliceG1 测试记录

- 日期：`2026-02-23`
- 切片：`Stage G - Slice G1`
- 目标：`SimHandoffEnvelope（Stage G 输入信封）完整性校验 -> StageGExecuteIntent（Stage G 入口意图包，validate-only）桥接`
- 范围说明：固定 `validate_only` 模式与 `write_enabled=false`，仅做 Stage G 入口准入校验与交接封装，不触发真实资产写入。

## 1. 本地实现与验证（助手）

- 已完成：
  - Runtime 新增 `StageGExecuteIntent` 契约 / 桥接 / JSON 序列化器。
  - Editor 新增命令：
    - `HCI.AgentExecutePlanReviewPrepareStageGExecuteIntent`
    - `HCI.AgentExecutePlanReviewPrepareStageGExecuteIntentJson`
  - 支持 `tamper=none|digest|apply|review|confirm|receipt|final|archive|handoff|ready` 模拟篡改/阻断。
- 编译：`Build.bat HCIEditorGenEditor Win64 Development ...` ✅ 通过
- 自动化：
  - `HCI.Editor.AgentExecutorStageGExecuteIntent` ✅ `4/4`
  - `HCI.Editor.AgentExecutorSimHandoffEnvelope`（回归）✅ `4/4`
- 证据日志：
  - `Saved/Logs/Automation_G1_StageGExecuteIntent.log`
  - `Saved/Logs/Automation_G1_SimHandoffEnvelopeRegression.log`

## 2. UE 手测步骤（用户执行）

### 2.1 先生成可通过链路（到 F15）

1. `HCI.AgentExecutePlanReviewDemo ok_level_risk`
2. `HCI.AgentExecutePlanReviewSelect 0`
3. `HCI.AgentExecutePlanReviewPrepareApply`
4. `HCI.AgentExecutePlanReviewPrepareConfirm 1 none`
5. `HCI.AgentExecutePlanReviewPrepareExecuteTicket none`
6. `HCI.AgentExecutePlanReviewPrepareSimExecuteReceipt none`
7. `HCI.AgentExecutePlanReviewPrepareSimFinalReport none`
8. `HCI.AgentExecutePlanReviewPrepareSimArchiveBundle none`
9. `HCI.AgentExecutePlanReviewPrepareSimHandoffEnvelope none`

### 2.2 G1 正常桥接与 JSON

10. `HCI.AgentExecutePlanReviewPrepareStageGExecuteIntent none`
11. `HCI.AgentExecutePlanReviewPrepareStageGExecuteIntentJson none`

### 2.3 G1 篡改/阻断校验

12. `HCI.AgentExecutePlanReviewPrepareStageGExecuteIntent digest`
13. `HCI.AgentExecutePlanReviewPrepareStageGExecuteIntent confirm`
14. `HCI.AgentExecutePlanReviewPrepareStageGExecuteIntent handoff`
15. `HCI.AgentExecutePlanReviewPrepareStageGExecuteIntent ready`

### 2.4 未确认链路（应拦截 E4005）

16. `HCI.AgentExecutePlanReviewPrepareConfirm 0 none`
17. `HCI.AgentExecutePlanReviewPrepareExecuteTicket none`
18. `HCI.AgentExecutePlanReviewPrepareSimExecuteReceipt none`
19. `HCI.AgentExecutePlanReviewPrepareSimFinalReport none`
20. `HCI.AgentExecutePlanReviewPrepareSimArchiveBundle none`
21. `HCI.AgentExecutePlanReviewPrepareSimHandoffEnvelope none`
22. `HCI.AgentExecutePlanReviewPrepareStageGExecuteIntent none`

## 3. 预期结果（Pass 判定标准）

1. 合法命令无 `Error`
- 阻断场景出现 `Warning` 可接受。

2. `PrepareStageGExecuteIntent` 摘要字段完整
- `[HCIAbilityKit][AgentExecutorStageGExecuteIntent] summary ...`
- 必含：
  - `request_id`
  - `sim_handoff_envelope_id`
  - `sim_archive_bundle_id`
  - `sim_final_report_id`
  - `sim_execute_receipt_id`
  - `execute_ticket_id`
  - `confirm_request_id`
  - `apply_request_id`
  - `review_request_id`
  - `selection_digest`
  - `archive_digest`
  - `handoff_digest`
  - `execute_intent_digest`
  - `execute_target`
  - `handoff_target`
  - `terminal_status`
  - `archive_status`
  - `handoff_status`
  - `stage_g_status`
  - `write_enabled`
  - `ready_for_stage_g_entry`
  - `error_code`
  - `reason`
  - `validation=ok`

3. 正常场景命中（`none`）
- `ready_for_stage_g_entry=true`
- `stage_g_status=ready`
- `write_enabled=false`
- `error_code=-`
- `reason=stage_g_execute_intent_ready_validate_only`

4. 行日志保留定位字段
- `tool_name=ScanLevelMeshRisks`
- `object_type=actor`
- `locate_strategy=camera_focus`
- `evidence_key=actor_path`

5. 阻断场景命中
- `digest -> E4202 / selection_digest_mismatch`
- `confirm -> E4202 / confirm_request_id_mismatch`
- `handoff -> E4209 / sim_handoff_envelope_not_ready`
- `ready -> E4205 / execute_ticket_not_ready`
- 未确认链路 -> `E4005 / user_not_confirmed`

6. `PrepareStageGExecuteIntentJson` 输出 JSON 且包含
- `"request_id"`, `"sim_handoff_envelope_id"`, `"sim_archive_bundle_id"`, `"sim_final_report_id"`, `"sim_execute_receipt_id"`
- `"execute_ticket_id"`, `"confirm_request_id"`, `"apply_request_id"`, `"review_request_id"`
- `"selection_digest"`, `"archive_digest"`, `"handoff_digest"`, `"execute_intent_digest"`
- `"execute_target"`, `"handoff_target"`
- `"transaction_mode"`, `"termination_policy"`, `"terminal_status"`, `"archive_status"`, `"handoff_status"`, `"stage_g_status"`
- `"write_enabled"`, `"ready_for_stage_g_entry"`, `"error_code"`, `"reason"`
- `"summary"`, `"items"`, `"blocked"`, `"skip_reason"`, `"locate_strategy"`

## 4. 实际结果

- 用户 UE 手测结果：`Pass`
- 证据要点（用户反馈）：
  - 正常场景：`execute_target=stage_g_execute_runtime`，并命中 `write_enabled=false` 与 `reason=stage_g_execute_intent_ready_validate_only`，证明已进入 Stage G 入口但仍保持 validate-only 安全锁定。
  - `handoff` 篡改：命中 `E4209 / sim_handoff_envelope_not_ready`（无 F15 合法信封不允许进入 Stage G）。
  - 撤销确认后链路末端：命中 `E4005 / user_not_confirmed`（全链路确认回溯拦截生效）。
  - 日志保留 `ScanLevelMeshRisks + actor_path + locate_strategy=camera_focus` 业务定位字段。
- 结论：`Pass`
