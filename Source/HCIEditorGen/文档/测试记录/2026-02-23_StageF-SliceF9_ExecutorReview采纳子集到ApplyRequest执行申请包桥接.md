# StageF-SliceF9 ExecutorReview 采纳子集 -> ApplyRequest 执行申请包桥接（2026-02-23）

- 日期：2026-02-23
- 切片编号：StageF-SliceF9
- 测试人：助手（实现/编译/自动化）+ 用户（UE 手测）
- 当前状态：已通过（用户 UE 手测 Pass）

## 1. 测试目标

- 将 F8 的“已采纳子集”审阅预览（`DryRunDiffReport`）桥接为 `ApplyRequest` 执行申请包（含 `selection_digest/ready/blocked_rows`）。
- 输出可确认、可追踪的摘要/行日志与 JSON，不触发真实资产写入（`simulate_dry_run`）。
- 保持 F6/F7/F8 审阅链路命令与日志口径不回归。

## 2. 影响范围

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Agent/HCIAbilityKitAgentApplyRequest.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Agent/HCIAbilityKitAgentApplyRequestJsonSerializer.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Agent/HCIAbilityKitAgentExecutorApplyRequestBridge.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Agent/HCIAbilityKitAgentApplyRequestJsonSerializer.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Agent/HCIAbilityKitAgentExecutorApplyRequestBridge.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Commands/HCIAbilityKitAgentDemoConsoleCommands.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Commands/HCIAbilityKitAgentDemoConsoleCommands.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Tests/HCIAbilityKitAgentExecutorApplyTests.cpp`
- `Source/HCIEditorGen/文档/00_总进度.md`
- `Source/HCIEditorGen/文档/03_Schema_v1与错误契约.md`
- `Source/HCIEditorGen/文档/04_代码框架总览_树状图.md`
- `Source/HCIEditorGen/文档/05_开发执行总方案_资产审计.md`
- `AGENTS.md`

## 3. 前置条件

- `StageF-SliceF8` 已通过。
- 工程可编译。
- UE 编辑器可执行控制台命令。

## 4. 操作步骤（UE 手测）

1. 生成关卡排雷审阅预览（Actor）
   - `HCIAbilityKit.AgentExecutePlanReviewDemo ok_level_risk`
2. （可选）筛选为单行，确保使用 F8 的“已采纳子集”
   - `HCIAbilityKit.AgentExecutePlanReviewSelect 0`
3. 生成 ApplyRequest 摘要/行预览
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareApply`
4. 生成 ApplyRequest JSON
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareApplyJson`
5. 生成预检阻断审阅预览（Asset，确认门禁失败）
   - `HCIAbilityKit.AgentExecutePlanReviewDemo fail_confirm`
6. 再生成 ApplyRequest（应 `ready=false`）
   - `HCIAbilityKit.AgentExecutePlanReviewPrepareApply`

## 5. 预期结果（Pass 判定标准）

1. 合法命令无 `Error`
   - 步骤 1~6 在合法参数下无 `Error`（阻断场景出现 `Warning` 属预期）
2. `PrepareApply` 摘要日志存在且字段完整
   - `[HCIAbilityKit][AgentExecutorApply] summary ...`
   - 包含：`request_id/review_request_id/selection_digest/total_rows/modifiable_rows/blocked_rows/ready/validation=ok`
3. `ok_level_risk` 后 `PrepareApply` 行命中（Actor）
   - `tool_name=ScanLevelMeshRisks`
   - `object_type=actor`
   - `locate_strategy=camera_focus`
   - `evidence_key=actor_path`
   - `blocked=false`
4. `fail_confirm` 后 `PrepareApply` 摘要命中阻断状态
   - `blocked_rows>=1`
   - `ready=false`
5. `fail_confirm` 后 `PrepareApply` 行命中（Asset 阻断项）
   - `tool_name=SetTextureMaxSize`
   - `blocked=true`
   - `skip_reason` 包含 `E4005`
   - `skip_reason` 包含 `confirm_gate`
   - `object_type=asset`
   - `locate_strategy=sync_browser`
6. `PrepareApplyJson` 输出 JSON 且包含
   - `"request_id"`
   - `"review_request_id"`
   - `"selection_digest"`
   - `"ready"`
   - `"summary"`
   - `"items"`
   - `"blocked"`
   - `"skip_reason"`
   - `"locate_strategy"`
7. F6/F7/F8 回归不破坏
   - `AgentExecutePlanReviewDemo / ReviewSelect / ReviewLocate` 仍可正常使用，日志前缀与字段口径不变

## 6. 实际结果（助手本地验证）

- 编译：通过
  - `Build.bat HCIEditorGenEditor Win64 Development ...` 成功。
- 自动化：通过
  - `HCIAbilityKit.Editor.AgentExecutorApply`：3/3 成功（F9 新增）
    - `BlockedRowsMakeApplyRequestNotReady`
    - `JsonIncludesDigestReadyAndBlockedFields`
    - `ReadyWhenNoBlockedRows`
  - `HCIAbilityKit.Editor.AgentExecutorReview`：通过（含 F6/F7/F8 审阅链路回归）
  - `HCIAbilityKit.Editor.AgentExecutorReviewLocate`：2/2 成功（F7 回归）
  - `HCIAbilityKit.Editor.AgentExecutorReviewSelect`：3/3 成功（F8 回归）
- 留证日志：
  - `Saved/Logs/Automation_F9_AgentExecutorApply_serial.log`
  - `Saved/Logs/Automation_F9_AgentExecutorReview_serial.log`
  - `Saved/Logs/Automation_F9_AgentExecutorReviewLocate_serial.log`
  - `Saved/Logs/Automation_F9_AgentExecutorReviewSelect_serial.log`
- 说明：
  - `UnrealEditor-Cmd` 并行运行会出现 `UnrealEditorServer` 噪音日志（连接中断）；已改为串行重跑并以串行日志为准。
  - F9 仅桥接 `ApplyRequest` 预览与 JSON 输出，仍为 `simulate_dry_run`，未接入真实资产写入。

## 7. 结论（用户 UE 手测）

- UE 手测结果：`Pass`。
- 用户验收结论（摘要）：
  - 所有命令仅输出 `Display/Warning`，无 `Error`。
  - 两次 `PrepareApply` 摘要均包含 `request_id/review_request_id/selection_digest/total_rows/modifiable_rows/blocked_rows/ready` 且 `validation=ok`。
  - `ok_level_risk` 的 `PrepareApply` 行命中 `tool_name=ScanLevelMeshRisks`、`object_type=actor`、`locate_strategy=camera_focus`、`evidence_key=actor_path`、`blocked=false`。
  - `fail_confirm` 的 `PrepareApply` 摘要命中 `blocked_rows=1` 且 `ready=false`。
  - `fail_confirm` 的 `PrepareApply` 行命中 `tool_name=SetTextureMaxSize`、`blocked=true`，`skip_reason` 包含 `E4005` 与 `confirm_gate`，并保持 `object_type=asset`、`locate_strategy=sync_browser`。
  - `PrepareApplyJson` 输出 JSON 且包含 `request_id/review_request_id/selection_digest/ready/summary/items/blocked/skip_reason/locate_strategy`。
