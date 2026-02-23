# StageF-SliceF8 ExecutorReview 逐项采纳选择契约与过滤预览（2026-02-23）

- 日期：2026-02-23
- 切片编号：StageF-SliceF8
- 测试人：助手（实现/编译/自动化）+ 用户（UE 手测）
- 当前状态：已通过（用户 UE 手测 Pass）

## 1. 测试目标

- 在 F6/F7 的 `ExecutorReview` 审阅预览基础上，新增“逐项采纳”选择过滤能力（按 `row_index` 列表）。
- 支持去重、越界校验与子集 `DryRunDiffReport` 生成，并复用 E2 JSON 序列化器输出。
- 成功选择后覆盖“最新审阅预览状态”，以便直接复用 F7 `Locate` 命令。
- 保持 `simulate_dry_run`，不触发真实资产写入，不破坏 F6/F7 既有日志口径。

## 2. 影响范围

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Agent/HCIAbilityKitDryRunDiffSelection.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Agent/HCIAbilityKitDryRunDiffSelection.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Commands/HCIAbilityKitAgentDemoConsoleCommands.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Commands/HCIAbilityKitAgentDemoConsoleCommands.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Tests/HCIAbilityKitAgentExecutorReviewSelectionTests.cpp`
- `Source/HCIEditorGen/文档/00_总进度.md`
- `Source/HCIEditorGen/文档/03_Schema_v1与错误契约.md`
- `Source/HCIEditorGen/文档/04_代码框架总览_树状图.md`
- `Source/HCIEditorGen/文档/05_开发执行总方案_资产审计.md`
- `AGENTS.md`

## 3. 前置条件

- `StageF-SliceF7` 已通过。
- 工程可编译。
- UE 编辑器可执行控制台命令。

## 4. 操作步骤

1. 编译（助手已完成）
   - `Build.bat HCIEditorGenEditor Win64 Development -Project=... -WaitMutex -FromMSBuild`
2. 自动化测试（助手已完成）
   - `HCIAbilityKit.Editor.AgentExecutorReviewSelect`（F8 新增）
   - `HCIAbilityKit.Editor.AgentExecutorReviewLocate`（F7 回归）
   - `HCIAbilityKit.Editor.AgentExecutorReview`（F6 回归）
3. UE 手测（生成 Actor 审阅预览）
   - `HCIAbilityKit.AgentExecutePlanReviewDemo ok_level_risk`
4. UE 手测（选择行，重复输入用于验证去重）
   - `HCIAbilityKit.AgentExecutePlanReviewSelect 0,0`
5. UE 手测（复用 F7 定位，验证选择后预览状态已替换）
   - `HCIAbilityKit.AgentExecutePlanReviewLocate 0`
6. UE 手测（生成预检阻断审阅预览）
   - `HCIAbilityKit.AgentExecutePlanReviewDemo fail_confirm`
7. UE 手测（选择并输出 JSON）
   - `HCIAbilityKit.AgentExecutePlanReviewSelectJson 0`
8. UE 手测（越界）
   - `HCIAbilityKit.AgentExecutePlanReviewSelect 99`

## 5. 预期结果（Pass 判定标准）

1. 合法命令无 `Error`
   - 步骤 3~7 在合法参数下无 `Error`（可出现 `Warning`，如阻断项 `skip_reason`）
   - 步骤 8 越界出现 `Error` 属预期
2. `ReviewSelect 0,0` 摘要命中（去重生效）
   - `[HCIAbilityKit][AgentExecutorReviewSelect] summary ...`
   - `input_rows=2`
   - `unique_rows=1`
   - `dropped_duplicates=1`
   - `total_after=1`
   - `validation=ok`
3. `ok_level_risk` 选择结果行命中
   - `tool_name=ScanLevelMeshRisks`
   - `object_type=actor`
   - `locate_strategy=camera_focus`
   - `evidence_key=actor_path`
4. 选择后 `ReviewLocate 0` 仍可定位（复用 F7）
   - `strategy=camera_focus`
   - `object_type=actor`
   - `success=true` 或 `success=false reason=actor_not_found`
5. `fail_confirm` 后 `ReviewSelectJson 0` 命中
   - 选择结果行：`tool_name=SetTextureMaxSize`
   - `skip_reason` 包含 `E4005`
   - `skip_reason` 包含 `confirm_gate`
   - `object_type=asset`
   - `locate_strategy=sync_browser`
6. `ReviewSelectJson 0` 输出 JSON 且包含
   - `"request_id"`
   - `"summary"`
   - `"diff_items"`
   - `"skip_reason"`
   - `"locate_strategy"`
7. 越界命令命中
   - `[HCIAbilityKit][AgentExecutorReviewSelect] invalid_args`
   - `error_code=E4201`
   - `reason=row_index_out_of_range`
   - `total=...`
8. F6/F7 回归不破坏
   - `AgentExecutePlanReviewDemo ...` 仍输出 `AgentExecutorReview summary + row=`
   - `AgentExecutePlanReviewLocate` 行为与 F7 口径一致

## 6. 实际结果（助手本地验证）

- 编译：通过
  - `Build.bat HCIEditorGenEditor Win64 Development ...` 成功。
- 自动化：通过
  - `HCIAbilityKit.Editor.AgentExecutorReviewSelect`：3/3 成功（F8 新增）
    - `SelectedJsonKeepsLocateAndSkipReasonFields`
    - `SelectRowsDedupPreservesOrder`
    - `SelectRowsRejectsOutOfRange`
  - `HCIAbilityKit.Editor.AgentExecutorReviewLocate`：2/2 成功（F7 回归）
  - `HCIAbilityKit.Editor.AgentExecutorReview`：通过（含 F6/F7/F8 前缀覆盖回归）
- 说明：
  - `UnrealEditor-Cmd` 在当前环境无稳定实时回显，已使用 `-abslog` 留证。
  - F8 新增 Runtime 过滤器为纯数据转换，不依赖 Editor-only 模块。
- UE 手测：通过（用户反馈满足全部门禁）
  - 标准 1：合法命令无 `Error`，越界命令 `ReviewSelect 99` 出现预期 `Error`（`E4201 / row_index_out_of_range`）。
  - 标准 2：`ReviewSelect 0,0` 去重生效，摘要命中 `input_rows=2 unique_rows=1 dropped_duplicates=1 total_after=1 validation=ok`。
  - 标准 3：`ok_level_risk` 选择结果行为 Actor 审阅项，命中 `tool_name=ScanLevelMeshRisks object_type=actor locate_strategy=camera_focus evidence_key=actor_path`。
  - 标准 4：选择后执行 `ReviewLocate 0` 复用 F7 成功，定位日志命中 `strategy=camera_focus object_type=actor`（样例 Actor 不存在时 `actor_not_found` Warning 属可接受范围）。
  - 标准 5：`fail_confirm` 后执行 `ReviewSelectJson 0`，行级日志命中 `tool_name=SetTextureMaxSize`，`skip_reason` 包含 `E4005` 与 `confirm_gate`，并保持 `object_type=asset locate_strategy=sync_browser`。
  - 标准 6：`ReviewSelectJson 0` 输出完整 JSON，结构包含 `request_id/summary/diff_items/skip_reason/locate_strategy`。
  - 标准 7：越界命令日志命中 `[HCIAbilityKit][AgentExecutorReviewSelect] invalid_args error_code=E4201 reason=row_index_out_of_range total=...`。
  - 标准 8：F6/F7 回归未破坏，`AgentExecutePlanReviewDemo` 仍输出 `AgentExecutorReview summary + row=`，`AgentExecutePlanReviewLocate` 行为口径保持一致。

## 7. 结论

- `StageF-SliceF8 Pass`
- 允许进入 `StageF-SliceF9`（待定义/待冻结）

## 8. 证据

- 构建日志：`C:\Users\50428\AppData\Local\UnrealBuildTool\Log.txt`
- 自动化日志（本轮）：
  - `Saved/Logs/Automation_F8_AgentExecutorReviewSelect.log`
  - `Saved/Logs/Automation_F8_AgentExecutorReviewLocate.log`
  - `Saved/Logs/Automation_F8_AgentExecutorReview.log`
