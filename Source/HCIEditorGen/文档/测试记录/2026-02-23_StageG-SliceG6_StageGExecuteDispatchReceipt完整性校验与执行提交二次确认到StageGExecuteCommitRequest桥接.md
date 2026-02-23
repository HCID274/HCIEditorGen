# 测试记录：StageG-SliceG6 StageGExecuteDispatchReceipt -> StageGExecuteCommitRequest（待验证）

- 日期：2026-02-23
- 切片编号：Stage G - Slice G6
- 测试人：用户（UE 手测）/ 助手（实现与回归）

## 1. 测试目标

- 验证 `StageGExecuteDispatchReceipt` 完整性校验 + `execute_commit_confirmed` 二次确认门禁已接入。
- 验证 `StageGExecuteCommitRequest` dry-run 桥接、JSON 输出与错误码收敛（`E4214/E4202/E4005`）。
- 验证 G5 回归未破坏（审计定位字段继续穿透到 G6）。

## 2. 影响范围

- Runtime：`StageGExecuteCommitRequest` 契约/桥接/JSON 序列化。
- Editor：`AgentDemoConsoleCommands` 新增 `...PrepareStageGExecuteCommitRequest` / `...Json` 命令。
- Tests：新增 `HCIAbilityKit.Editor.AgentExecutorStageGExecuteCommitRequest` 自动化测试；修复 G5 DispatchReceipt 测试 helper 命名冲突（Unity 构建）。

## 3. 前置条件

- 已能在 UE 内执行到 `StageG-SliceG5`（`StageGExecuteDispatchReceipt`）命令链。
- 项目可正常打开并加载插件 `HCIAbilityKit`。

## 4. 操作步骤（UE 手测）

1. 先跑通到 `StageGExecuteDispatchReceipt` 的可通过链路（`...ReviewDemo -> ...Select -> ...PrepareApply -> ...PrepareConfirm -> ...PrepareExecuteTicket -> ...PrepareSimExecuteReceipt -> ...PrepareSimFinalReport -> ...PrepareSimArchiveBundle -> ...PrepareSimHandoffEnvelope -> ...PrepareStageGExecuteIntent -> ...PrepareStageGWriteEnableRequest 1 none -> ...PrepareStageGExecutePermitTicket 1 none -> ...PrepareStageGExecuteDispatchRequest 1 none -> ...PrepareStageGExecuteDispatchReceipt none`）。
2. 执行正常场景：`HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteCommitRequest 1 none`。
3. 执行 JSON 输出：`HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteCommitRequestJson 1 none`。
4. 执行阻断场景：
   - `...PrepareStageGExecuteCommitRequest 0 none`
   - `...PrepareStageGExecuteCommitRequest 1 digest`
   - `...PrepareStageGExecuteCommitRequest 1 intent`
   - `...PrepareStageGExecuteCommitRequest 1 handoff`
   - `...PrepareStageGExecuteCommitRequest 1 dispatch`
   - `...PrepareStageGExecuteCommitRequest 1 receipt`
   - `...PrepareStageGExecuteCommitRequest 1 ready`

## 5. 预期结果

- 正常场景：`stage_g_execute_commit_request_ready=true`、`stage_g_execute_commit_request_status=ready`、`error_code=-`、`reason=stage_g_execute_commit_request_ready`。
- 阻断场景：
  - `0 none -> E4005 / stage_g_execute_commit_not_confirmed`
  - `digest -> E4202 / selection_digest_mismatch`
  - `intent -> E4202 / stage_g_execute_intent_id_mismatch`
  - `handoff -> E4202 / sim_handoff_envelope_id_mismatch`
  - `dispatch -> E4202 / stage_g_execute_dispatch_request_id_mismatch`
  - `receipt -> E4214 / stage_g_execute_dispatch_receipt_not_ready`
  - `ready -> E4214 / stage_g_execute_dispatch_receipt_not_ready`
- 行日志持续保留 `tool_name/object_type/locate_strategy/evidence_key/actor_path`。
- JSON 输出包含 `stage_g_execute_dispatch_receipt_id/stage_g_execute_commit_request_digest/stage_g_execute_commit_request_status/stage_g_execute_commit_request_ready/execute_commit_confirmed` 与 `summary/items/blocked/skip_reason/locate_strategy`。

## 6. 实际结果

- 待用户 UE 手测填写。

## 7. 结论

- 待判定（`Pass` / `Fail`）。

## 8. 证据

- 本地编译：`Build.bat HCIEditorGenEditor Win64 Development ...`（已通过）
- 自动化日志：
  - `Saved/Logs/Automation_G6_StageGExecuteCommitRequest_serial.log`
  - `Saved/Logs/Automation_G6_StageGExecuteDispatchReceiptRegression_serial.log`
- UE 手测日志：待用户提供

## 9. 问题与后续动作

- 如 `Fail`：优先记录 `tamper` 场景、错误码、`reason` 与摘要字段缺失项。
- 如 `Pass`：转正本记录并切换主线到 `Stage G - Slice G7`。
