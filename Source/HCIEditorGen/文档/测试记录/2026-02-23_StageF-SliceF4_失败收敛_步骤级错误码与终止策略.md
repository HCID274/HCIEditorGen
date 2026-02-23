# StageF-SliceF4 失败收敛（步骤级错误码与终止策略）（2026-02-23）

- 日期：2026-02-23
- 切片编号：StageF-SliceF4
- 测试人：助手（实现/编译/自动化）+ 用户（UE 手测）
- 当前状态：已通过（用户 UE 手测 Pass）

## 1. 测试目标

- 扩展 `FHCIAbilityKitAgentExecutor` 的失败收敛能力：
  - 步骤级失败错误码与原因收敛；
  - 顶层失败步骤元数据（`failed_step_index/failed_step_id/failed_tool_name`）；
  - 终止策略 `stop_on_first_failure / continue_on_failure`；
  - `stop_on_first_failure` 下后续步骤输出 `status=skipped` 行。
- 新增 F4 UE 演示命令 `HCIAbilityKit.AgentExecutePlanFailDemo`，用于验证失败场景日志口径。
- 保持 F3/F2 主链路与计划契约不回归。

## 2. 影响范围

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Agent/HCIAbilityKitAgentExecutor.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Agent/HCIAbilityKitAgentExecutor.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Commands/HCIAbilityKitAgentDemoConsoleCommands.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Commands/HCIAbilityKitAgentDemoConsoleCommands.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Tests/HCIAbilityKitAgentExecutorTests.cpp`
- `Source/HCIEditorGen/文档/00_总进度.md`
- `Source/HCIEditorGen/文档/03_Schema_v1与错误契约.md`
- `AGENTS.md`

## 3. 前置条件

- `StageF-SliceF3` 已通过。
- 工程可编译。
- UE 编辑器可打开并执行控制台命令。

## 4. 操作步骤

1. 编译（助手已完成）
   - `Build.bat HCIEditorGenEditor Win64 Development -Project=... -WaitMutex -FromMSBuild`
2. 自动化测试（助手已完成）
   - `HCIAbilityKit.Editor.AgentExecutor`（含 F4 新用例）
   - `HCIAbilityKit.Editor.AgentPlan`（前缀回归）
   - `HCIAbilityKit.Editor.AgentPlanValidation`（F2 回归）
3. UE 手测（默认三案例摘要）
   - `HCIAbilityKit.AgentExecutePlanFailDemo`
4. UE 手测（Stop 策略失败单案例）
   - `HCIAbilityKit.AgentExecutePlanFailDemo fail_stop`
5. UE 手测（Continue 策略失败单案例）
   - `HCIAbilityKit.AgentExecutePlanFailDemo fail_continue`

## 5. 预期结果（Pass 判定标准）

1. 上述 3 条 UE 命令（步骤 3~5）在合法参数下均无 `Error` 级日志。
2. `HCIAbilityKit.AgentExecutePlanFailDemo`（无参）输出：
   - 3 条案例前置日志（`[HCIAbilityKit][AgentExecutorFail] case=...`）
   - 每案例 `AgentExecutor` 的 `summary + row=`
   - 1 条 F4 总摘要，且包含：
     - `summary total_cases=3`
     - `stop_policy_cases=2`
     - `continue_policy_cases=1`
     - `execution_mode=simulate_dry_run`
     - `validation=ok`
3. F4 单案例/默认案例的 `AgentExecutor summary` 行需包含新增字段：
   - `termination_policy`
   - `skipped_steps`
   - `failed_step_index`
   - `failed_step_id`
   - `failed_tool_name`
4. `fail_stop` 场景需命中（`stop_on_first_failure`）：
   - `termination_policy=stop_on_first_failure`
   - `completed=false`
   - `terminal_status=failed`
   - `terminal_reason=executor_step_failed_stop_on_first_failure`
   - `failed_steps=1`
   - `skipped_steps=1`
   - `error_code=E4101`
5. `fail_stop` 场景的 `row=` 至少包含：
   - 失败行：`status=failed attempted=true succeeded=false error_code=E4101`
   - 跳过行：`status=skipped attempted=false succeeded=false reason=terminated_by_stop_on_first_failure`
6. `fail_continue` 场景需命中（`continue_on_failure`）：
   - `termination_policy=continue_on_failure`
   - `completed=false`
   - `terminal_status=completed_with_failures`
   - `terminal_reason=executor_step_failed_continue_on_failure`
   - `failed_steps=1`
   - `skipped_steps=0`
   - `executed_steps=2`
   - `error_code=E4102`
7. `fail_continue` 场景 `row=` 需同时包含：
   - 至少一条失败行（`status=failed`）
   - 至少一条成功行（`status=succeeded`）

## 6. 实际结果（助手本地验证）

- 编译：通过
  - `Build.bat HCIEditorGenEditor Win64 Development ...` 成功。
- 自动化：通过
  - `HCIAbilityKit.Editor.AgentExecutor`：5/5 成功
    - `ValidPlanProducesStepResults`
    - `InvalidPlanRejectedByValidator`
    - `LevelRiskPlanProducesExpectedEvidenceKeys`
    - `StepFailureStopOnFirstPolicyConverges`
    - `StepFailureContinuePolicyKeepsRunning`
  - `HCIAbilityKit.Editor.AgentPlan`：前缀回归成功（日志显示 Found 11，含 `AgentPlanValidation` 前缀测试）
  - `HCIAbilityKit.Editor.AgentPlanValidation`：8/8 成功（独立回归）
- 说明：
  - F4 开发中顺带修复了 `AgentDemoConsoleCommands.cpp` 与 `EditorModule.cpp` 在 Unity 编译下重复定义 `LogHCIAbilityKitAuditScan` 的构建问题（改为独立日志分类 `LogHCIAbilityKitAgentDemo`），不改变外部日志前缀字符串口径。
  - `UnrealEditor-Cmd` 在当前环境通常无控制台实时回显，以 `Saved/Logs/*.log` 为证据。
- UE 手测：通过（用户反馈 `Pass`）
  - `HCIAbilityKit.AgentExecutePlanFailDemo`（无参）无 Error，摘要命中 `total_cases=3 stop_policy_cases=2 continue_policy_cases=1 execution_mode=simulate_dry_run validation=ok`；
  - `AgentExecutor summary` 行包含 `termination_policy/skipped_steps/failed_step_index/failed_step_id/failed_tool_name`；
  - `fail_stop` 命中 `terminal_status=failed`、`E4101` 且含 `status=skipped` 行；
  - `fail_continue` 命中 `terminal_status=completed_with_failures`、`E4102` 且同时含失败行与成功行。

## 7. 结论

- `StageF-SliceF4` 已通过，允许进入 `StageF-SliceF5`（待定义）。

## 8. 证据

- 构建日志：`C:\Users\50428\AppData\Local\UnrealBuildTool\Log.txt`
- 自动化日志（分离留证）：
  - `Saved/Logs/Automation_F4_AgentExecutor.log`
  - `Saved/Logs/Automation_F4_AgentPlan.log`
  - `Saved/Logs/Automation_F4_AgentPlanValidation.log`

## 9. 问题与后续动作

- 已完成用户 UE 手测门禁，结果 `Pass`。
- 下一步：冻结 `StageF-SliceF5` 的切片目标后继续推进。
