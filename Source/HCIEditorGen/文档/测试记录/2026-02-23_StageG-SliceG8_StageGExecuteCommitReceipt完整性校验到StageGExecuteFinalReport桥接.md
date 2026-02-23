# 测试记录：StageG-SliceG8 StageGExecuteCommitReceipt -> StageGExecuteFinalReport（待验证）

- 日期：2026-02-23
- 切片编号：Stage G - Slice G8
- 测试人：用户（UE 手测）/ 助手（实现与回归）

## 1. 测试目标

- 验证 `StageGExecuteCommitReceipt` 完整性校验已接入，并可桥接为 `StageGExecuteFinalReport`（dry-run）。
- 验证 `StageGExecuteFinalReport` 的终态字段与错误码收敛（`E4216/E4202`，以及可选 `E4005` 穿透回溯）。
- 验证 G7 回归未破坏（审计定位字段继续穿透到 G8）。

## 2. 影响范围

- Runtime：`StageGExecuteFinalReport` 契约 / 桥接 / JSON 序列化。
- Editor：`AgentDemoConsoleCommands` 新增 `...PrepareStageGExecuteFinalReport` / `...Json` 命令。
- Tests：新增 `HCIAbilityKit.Editor.AgentExecutorStageGExecuteFinalReport` 自动化测试。

## 3. 前置条件

- 已能在 UE 内执行到 `StageG-SliceG7`（`StageGExecuteCommitReceipt`）命令链。
- 项目可正常打开并加载插件 `HCIAbilityKit`。

## 4. 操作步骤（UE 手测）

1. 先跑通到 `StageGExecuteCommitReceipt` 的可通过链路（`...ReviewDemo -> ...Select -> ...PrepareApply -> ...PrepareConfirm -> ...PrepareExecuteTicket -> ...PrepareSimExecuteReceipt -> ...PrepareSimFinalReport -> ...PrepareSimArchiveBundle -> ...PrepareSimHandoffEnvelope -> ...PrepareStageGExecuteIntent -> ...PrepareStageGWriteEnableRequest 1 none -> ...PrepareStageGExecutePermitTicket 1 none -> ...PrepareStageGExecuteDispatchRequest 1 none -> ...PrepareStageGExecuteDispatchReceipt none -> ...PrepareStageGExecuteCommitRequest 1 none -> ...PrepareStageGExecuteCommitReceipt none`）。
2. 执行正常场景：`HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteFinalReport none`。
3. 执行 JSON 输出：`HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteFinalReportJson none`。
4. 执行阻断场景：
   - `...PrepareStageGExecuteFinalReport digest`
   - `...PrepareStageGExecuteFinalReport intent`
   - `...PrepareStageGExecuteFinalReport handoff`
   - `...PrepareStageGExecuteFinalReport dispatch`
   - `...PrepareStageGExecuteFinalReport receipt`
   - `...PrepareStageGExecuteFinalReport commit`
   - `...PrepareStageGExecuteFinalReport commitreceipt`
   - `...PrepareStageGExecuteFinalReport ready`
5. （可选）验证未确认链路穿透：
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteCommitRequest 0 none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteCommitReceipt none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteFinalReport none`

## 5. 预期结果

- 正常场景：
  - `stage_g_execute_final_report_status=completed`
  - `stage_g_execute_finalized=true`
  - `stage_g_execute_final_report_ready=true`
  - `error_code=-`
  - `reason=stage_g_execute_final_report_ready`
- 阻断场景：
  - `digest -> E4202 / selection_digest_mismatch`
  - `intent -> E4202 / stage_g_execute_intent_id_mismatch`
  - `handoff -> E4202 / sim_handoff_envelope_id_mismatch`
  - `dispatch -> E4202 / stage_g_execute_dispatch_request_id_mismatch`
  - `receipt -> E4202 / stage_g_execute_dispatch_receipt_id_mismatch`
  - `commit -> E4202 / stage_g_execute_commit_request_id_mismatch`
  - `commitreceipt -> E4202 / stage_g_execute_commit_receipt_id_mismatch`
  - `ready -> E4216 / stage_g_execute_commit_receipt_not_ready`
  - （可选）未确认链路：`E4005 / stage_g_execute_commit_not_confirmed`
- 行日志持续保留 `tool_name/object_type/locate_strategy/evidence_key/actor_path`。
- JSON 输出包含：
  - `stage_g_execute_commit_receipt_id`
  - `stage_g_execute_commit_receipt_digest`
  - `stage_g_execute_final_report_digest`
  - `stage_g_execute_final_report_status`
  - `stage_g_execute_finalized`
  - `stage_g_execute_final_report_ready`
  - `summary/items/blocked/skip_reason/locate_strategy`

## 6. 实际结果

- 用户 UE 手测通过。
- 正常场景（`none`）命中：
  - `stage_g_execute_finalized=true`
  - `stage_g_execute_final_report_status=completed`
  - `stage_g_execute_final_report_ready=true`
  - `error_code=-`
  - `reason=stage_g_execute_final_report_ready`
- 终态 JSON 尺寸约 `3567 bytes`，包含从 `selection_digest` 到 `stage_g_execute_final_report_digest` 的全路径递归摘要与多级 ID 链。
- 可选未确认链路穿透验证通过：
  - 在 `CommitRequest 0 none` 后继续尝试生成 FinalReport，命中 `E4005 / stage_g_execute_commit_not_confirmed`。
- 篡改/阻断场景验证通过：
  - `digest -> E4202 / selection_digest_mismatch`
  - `intent -> E4202 / stage_g_execute_intent_id_mismatch`
  - `handoff -> E4202 / sim_handoff_envelope_id_mismatch`
  - `dispatch -> E4202 / stage_g_execute_dispatch_request_id_mismatch`
  - `receipt -> E4202 / stage_g_execute_dispatch_receipt_id_mismatch`
  - `commit -> E4202 / stage_g_execute_commit_request_id_mismatch`
  - `commitreceipt -> E4202 / stage_g_execute_commit_receipt_id_mismatch`
  - `ready -> E4216 / stage_g_execute_commit_receipt_not_ready`
- 行日志与 JSON 持续保留审计定位字段（`ScanLevelMeshRisks`、`actor_path`、`locate_strategy=camera_focus`）。

## 7. 结论

- `Pass`

## 8. 证据

- 本地编译：`Build.bat HCIEditorGenEditor Win64 Development ...`（已通过）
- 自动化日志（如环境噪音导致 UE-Cmd 留证不稳定，以 UE 手测门禁为主并在此注明）：
  - `Saved/Logs/Automation_G8_StageGExecuteFinalReport_serial.log`（4/4，通过）
  - `Saved/Logs/Automation_G8_StageGExecuteCommitReceiptRegression_serial.log`（G7 回归 4/4，通过）
- UE 手测日志：用户已在聊天中给出 Pass 结论（终局状态闭环、E4005 穿透门禁、递归摘要链与审计定位字段均通过）。

## 9. 问题与后续动作

- 如 `Fail`：优先记录 `tamper` 场景、错误码、`reason` 与摘要字段缺失项。
- 如 `Pass`：转正本记录并切换主线到 `Stage G - Slice G9`（已执行）。
