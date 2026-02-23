# StageF-SliceF7 ExecutorReview 定位闭环（2026-02-23）

- 日期：2026-02-23
- 切片编号：StageF-SliceF7
- 测试人：助手（实现/编译/自动化）+ 用户（UE 手测）
- 当前状态：已通过（用户 UE 手测 Pass）

## 1. 测试目标

- 在 F6 `Executor -> DryRunDiff` 审阅桥接基础上，新增按行定位命令 `HCIAbilityKit.AgentExecutePlanReviewLocate [row_index]`。
- 打通 `计划 -> 执行 -> 审阅 -> 定位` 最小交互闭环（仍为 `simulate_dry_run`，不触发真实资产写入）。
- 保持 F6 `ReviewDemo/ReviewJson` 既有日志字段口径不变。
- 覆盖两类定位策略：
  - `actor -> camera_focus`
  - `asset -> sync_browser`

## 2. 影响范围

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Commands/HCIAbilityKitAgentDemoConsoleCommands.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Commands/HCIAbilityKitAgentDemoConsoleCommands.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Commands/HCIAbilityKitAgentExecutorReviewLocateUtils.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Commands/HCIAbilityKitAgentExecutorReviewLocateUtils.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Tests/HCIAbilityKitAgentExecutorReviewLocateTests.cpp`
- `Source/HCIEditorGen/文档/00_总进度.md`
- `Source/HCIEditorGen/文档/03_Schema_v1与错误契约.md`
- `Source/HCIEditorGen/文档/04_代码框架总览_树状图.md`
- `Source/HCIEditorGen/文档/05_开发执行总方案_资产审计.md`
- `AGENTS.md`

## 3. 前置条件

- `StageF-SliceF6` 已通过。
- 工程可编译。
- UE 编辑器可打开并执行控制台命令。

## 4. 操作步骤

1. 编译（助手已完成）
   - `Build.bat HCIEditorGenEditor Win64 Development -Project=... -WaitMutex -FromMSBuild`
2. 自动化测试（助手已完成）
   - `HCIAbilityKit.Editor.AgentExecutorReviewLocate`（F7 新增）
   - `HCIAbilityKit.Editor.AgentExecutorReview`（F6 回归）
   - `HCIAbilityKit.Editor.AgentExecutor`（F3~F5 回归）
3. UE 手测（先生成 review 预览状态，Actor 案例）
   - `HCIAbilityKit.AgentExecutePlanReviewDemo ok_level_risk`
4. UE 手测（定位 Actor 行，通常 `row=0`）
   - `HCIAbilityKit.AgentExecutePlanReviewLocate 0`
5. UE 手测（生成预检阻断审阅行，Asset 案例）
   - `HCIAbilityKit.AgentExecutePlanReviewDemo fail_confirm`
6. UE 手测（定位 Asset 行，通常 `row=0`）
   - `HCIAbilityKit.AgentExecutePlanReviewLocate 0`
7. UE 手测（边界：越界行号）
   - `HCIAbilityKit.AgentExecutePlanReviewLocate 99`

## 5. 预期结果（Pass 判定标准）

1. 上述 UE 命令在合法参数下无 `Error`（步骤 7 越界案例允许 `Error`，属于预期边界校验日志）。
2. `ok_level_risk` 后执行 `AgentExecutePlanReviewLocate 0` 时，定位日志命中：
   - `[HCIAbilityKit][AgentExecutorReview] locate row=0`
   - `tool_name=ScanLevelMeshRisks`
   - `strategy=camera_focus`
   - `object_type=actor`
   - `success=true` 或 `success=false reason=actor_not_found`（场景不存在对应 Actor 时可接受）
3. `fail_confirm` 后执行 `AgentExecutePlanReviewLocate 0` 时，定位日志命中：
   - `tool_name=SetTextureMaxSize`
   - `strategy=sync_browser`
   - `object_type=asset`
   - `success=true` 或 `success=false reason=asset_load_failed`（若示例资产不存在于项目中可接受）
4. `AgentExecutePlanReviewLocate 99` 输出边界错误：
   - `locate_invalid_args`
   - `reason=row_index_out_of_range`
   - `total=...`
5. F6 命令回归不受影响（至少一条确认）：
   - `HCIAbilityKit.AgentExecutePlanReviewDemo ...` 仍正常输出 `AgentExecutorReview summary + row=`

## 6. 实际结果（助手本地验证）

- 编译：通过
  - `Build.bat HCIEditorGenEditor Win64 Development ...` 成功。
- 自动化：通过
  - `HCIAbilityKit.Editor.AgentExecutorReviewLocate`：2/2 成功（F7 新增）
    - `ResolveActorRowKeepsCameraFocusStrategy`
    - `ResolveRejectsOutOfRangeRow`
  - `HCIAbilityKit.Editor.AgentExecutorReview`：通过（F6 回归）
  - `HCIAbilityKit.Editor.AgentExecutor`：通过（F3~F5 回归，同时命中 Review 前缀相关测试属预期）
- 说明：
  - `UnrealEditor-Cmd` 在当前环境控制台常无实时回显，已使用 `-abslog` 取证。
- UE 手测：通过（用户反馈满足全部门禁）
  - 标准 1：合法命令无 `Error`，越界 `Locate 99` 出现 `Error(row_index_out_of_range)` 属预期。
  - 标准 2：`ok_level_risk -> Locate 0` 命中 `tool_name=ScanLevelMeshRisks strategy=camera_focus object_type=actor`，并出现 `success=false reason=actor_not_found`（允许场景无 Actor）。
  - 标准 3：`fail_confirm -> Locate 0` 命中 `tool_name=SetTextureMaxSize strategy=sync_browser object_type=asset`，并出现 `success=false reason=asset_load_failed`（允许示例资产不存在）。
  - 标准 4：越界日志命中 `locate_invalid_args reason=row_index_out_of_range row_index=99 total=1`。
  - 标准 5：F6 回归未破坏，`AgentExecutePlanReviewDemo` 仍输出 `AgentExecutorReview summary + row=`。

## 7. 结论

- `StageF-SliceF7 Pass`
- 允许进入 `StageF-SliceF8`（待定义/待冻结）

## 8. 证据

- 构建日志：`C:\Users\50428\AppData\Local\UnrealBuildTool\Log.txt`
- 自动化日志（本轮）：
  - `Saved/Logs/Automation_F7_AgentExecutorReviewLocate_serial.log`
  - `Saved/Logs/Automation_F7_AgentExecutorReview_serial.log`
  - `Saved/Logs/Automation_F7_AgentExecutor_serial.log`
