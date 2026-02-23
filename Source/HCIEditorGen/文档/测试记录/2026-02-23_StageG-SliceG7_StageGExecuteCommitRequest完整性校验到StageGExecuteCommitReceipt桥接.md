# 测试记录：StageG-SliceG7 StageGExecuteCommitRequest -> StageGExecuteCommitReceipt（待验证）

- 日期：2026-02-23
- 切片编号：Stage G - Slice G7
- 测试人：用户（UE 手测）/ 助手（实现与回归）

## 1. 测试目标

- 验证 `StageGExecuteCommitRequest` 完整性校验已接入，并可桥接为 `StageGExecuteCommitReceipt`（dry-run）。
- 验证 `StageGExecuteCommitReceipt` 的受理状态字段与错误码收敛（`E4215/E4202`，以及可选 `E4005` 穿透回溯）。
- 验证 G6 回归未破坏（审计定位字段继续穿透到 G7）。

## 2. 影响范围

- Runtime：`StageGExecuteCommitReceipt` 契约 / 桥接 / JSON 序列化。
- Editor：`AgentDemoConsoleCommands` 新增 `...PrepareStageGExecuteCommitReceipt` / `...Json` 命令。
- Tests：新增 `HCIAbilityKit.Editor.AgentExecutorStageGExecuteCommitReceipt` 自动化测试。

## 3. 前置条件

- 已能在 UE 内执行到 `StageG-SliceG6`（`StageGExecuteCommitRequest`）命令链。
- 项目可正常打开并加载插件 `HCIAbilityKit`。

## 4. 操作步骤（UE 手测）

1. 先跑通到 `StageGExecuteCommitRequest` 的可通过链路（`...ReviewDemo -> ...Select -> ...PrepareApply -> ...PrepareConfirm -> ...PrepareExecuteTicket -> ...PrepareSimExecuteReceipt -> ...PrepareSimFinalReport -> ...PrepareSimArchiveBundle -> ...PrepareSimHandoffEnvelope -> ...PrepareStageGExecuteIntent -> ...PrepareStageGWriteEnableRequest 1 none -> ...PrepareStageGExecutePermitTicket 1 none -> ...PrepareStageGExecuteDispatchRequest 1 none -> ...PrepareStageGExecuteDispatchReceipt none -> ...PrepareStageGExecuteCommitRequest 1 none`）。
2. 执行正常场景：`HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteCommitReceipt none`。
3. 执行 JSON 输出：`HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteCommitReceiptJson none`。
4. 执行阻断场景：
   - `...PrepareStageGExecuteCommitReceipt digest`
   - `...PrepareStageGExecuteCommitReceipt intent`
   - `...PrepareStageGExecuteCommitReceipt handoff`
   - `...PrepareStageGExecuteCommitReceipt dispatch`
   - `...PrepareStageGExecuteCommitReceipt receipt`
   - `...PrepareStageGExecuteCommitReceipt commit`
   - `...PrepareStageGExecuteCommitReceipt ready`
5. （可选）验证未确认链路穿透：
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteCommitRequest 0 none`
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteCommitReceipt none`

## 5. 预期结果

- 正常场景：
  - `stage_g_execute_commit_accepted=true`
  - `stage_g_execute_commit_receipt_ready=true`
  - `stage_g_execute_commit_receipt_status=accepted`
  - `error_code=-`
  - `reason=stage_g_execute_commit_receipt_ready`
- 阻断场景：
  - `digest -> E4202 / selection_digest_mismatch`
  - `intent -> E4202 / stage_g_execute_intent_id_mismatch`
  - `handoff -> E4202 / sim_handoff_envelope_id_mismatch`
  - `dispatch -> E4202 / stage_g_execute_dispatch_request_id_mismatch`
  - `receipt -> E4202 / stage_g_execute_dispatch_receipt_id_mismatch`
  - `commit -> E4202 / stage_g_execute_commit_request_id_mismatch`
  - `ready -> E4215 / stage_g_execute_commit_request_not_ready`
  - （可选）未确认链路：`E4005 / stage_g_execute_commit_not_confirmed`
- 行日志持续保留 `tool_name/object_type/locate_strategy/evidence_key/actor_path`。
- JSON 输出包含：
  - `stage_g_execute_commit_request_id`
  - `stage_g_execute_commit_request_digest`
  - `stage_g_execute_commit_receipt_digest`
  - `stage_g_execute_commit_receipt_status`
  - `stage_g_execute_commit_accepted`
  - `stage_g_execute_commit_receipt_ready`
  - `summary/items/blocked/skip_reason/locate_strategy`

## 6. 实际结果

- 待用户 UE 手测填写。

## 7. 结论

- 待判定（`Pass` / `Fail`）。

## 8. 证据

- 本地编译：`Build.bat HCIEditorGenEditor Win64 Development ...`（已通过）
- 自动化日志（环境噪音说明：UE-Cmd 退出期 `ScopeLock` 断言可能干扰稳定留证）：
  - `Saved/Logs/Automation_G7_StageGExecuteCommitReceipt.log`（如有）
  - 或 `Saved/Logs/HCIEditorGen.log` 中对应 `AgentExecutorStageGExecuteCommitReceipt` 条目
- UE 手测日志：待用户提供

## 9. 问题与后续动作

- 如 `Fail`：优先记录 `tamper` 场景、错误码、`reason` 与摘要字段缺失项。
- 如 `Pass`：转正本记录并切换主线到 `Stage G - Slice G8`。
