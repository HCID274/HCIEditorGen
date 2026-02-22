# StageE-SliceE5 事务策略 All-or-Nothing（2026-02-22）

- 日期：2026-02-22
- 切片编号：StageE-SliceE5
- 测试人：助手（实现/编译/自动化）+ 用户（UE 手测）
- 当前状态：已完成（用户 UE 手测通过）

## 1. 测试目标

- 落地事务策略 `All-or-Nothing` 最小执行骨架：
  - 全部步骤成功时整单提交；
  - 任一步骤失败时整单回滚；
  - 失败返回 `E4007`；
  - 不允许保留部分提交结果。
- 提供 UE 控制台演示入口，验证事务摘要与失败回滚日志字段。

## 2. 影响范围

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Agent/HCIAbilityKitAgentExecutionGate.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Agent/HCIAbilityKitAgentExecutionGate.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/HCIAbilityKitEditorModule.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Tests/HCIAbilityKitAgentExecutionGateTests.cpp`
- `Source/HCIEditorGen/文档/00_总进度.md`
- `Source/HCIEditorGen/文档/03_Schema_v1与错误契约.md`
- `Source/HCIEditorGen/文档/04_代码框架总览_树状图.md`
- `AGENTS.md`

## 3. 前置条件

- `StageE-SliceE4` 已通过。
- 工程编译通过。
- UE 编辑器可打开并执行控制台命令。

## 4. 操作步骤

1. 编译插件（助手已完成本地验证）
   - `Build.bat HCIEditorGenEditor Win64 Development -Project=... -WaitMutex -FromMSBuild`
2. 自动化测试（助手已完成本地验证）
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AgentExec; Quit"`（扩展至 9 条）
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AgentTools; Quit"`（回归）
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AgentDryRun; Quit"`（回归）
3. UE 手测（默认三案例摘要）
   - `HCIAbilityKit.AgentTransactionDemo`
4. UE 手测（自定义：全部成功，期望全提交）
   - `HCIAbilityKit.AgentTransactionDemo 3 0`
5. UE 手测（自定义：第 2 步失败，期望整单回滚）
   - `HCIAbilityKit.AgentTransactionDemo 3 2`

## 5. 预期结果

- `HCIAbilityKit.AgentTransactionDemo`（无参数）输出 3 条案例日志 + 1 条摘要日志：
  - 案例字段包含：
    - `request_id`
    - `transaction_mode`
    - `total_steps`
    - `executed_steps`
    - `committed_steps`
    - `rolled_back_steps`
    - `committed`
    - `rolled_back`
    - `failed_step_index`
    - `failed_step_id`
    - `failed_tool_name`
    - `error_code`
    - `reason`
  - 摘要日志包含：
    - `summary total_cases=3`
    - `transaction_mode=all_or_nothing`
    - `expected_failed_code=E4007`
    - `validation=ok`
- `HCIAbilityKit.AgentTransactionDemo 3 0` 命中：
  - `committed=true`
  - `rolled_back=false`
  - `committed_steps=3`
- `HCIAbilityKit.AgentTransactionDemo 3 2` 命中：
  - `committed=false`
  - `rolled_back=true`
  - `failed_step_index=2`
  - `error_code=E4007`
  - `reason=step_failed_all_or_nothing_rollback`
  - `committed_steps=0`（无部分提交）

## 6. 实际结果（助手本地验证）

- 编译：通过。
  - `Build.bat HCIEditorGenEditor Win64 Development ...` 成功。
- 自动化：通过。
  - `HCIAbilityKit.Editor.AgentExec`：9/9 成功
    - `ConfirmGateBlocksUnconfirmedWrite`
    - `ConfirmGateAllowsReadOnlyWithoutConfirm`
    - `ConfirmGateAllowsConfirmedWrite`
    - `BlastRadiusBlocksOverLimit`
    - `BlastRadiusAllowsAtLimit`
    - `BlastRadiusSkipsReadOnlyTool`
    - `TransactionCommitsAllStepsOnSuccess`
    - `TransactionRollsBackOnFailure`
    - `TransactionRejectsUnknownToolBeforeExecution`
  - `HCIAbilityKit.Editor.AgentTools`：3/3 成功（回归）
  - `HCIAbilityKit.Editor.AgentDryRun`：2/2 成功（回归）
- 说明（日志留证方式）：
  - 使用 `-abslog=` 分离日志文件，避免覆盖 `Saved/Logs/HCIEditorGen.log`。
  - 包装层偶发 `UnrealEditorServer-HCIEditorGen` 网络报错出现在 UE-Cmd 退出阶段，不属于插件测试失败依据；以自动化测试结果行（`Result={成功}`）为准。
- UE 手测：通过。
  - `HCIAbilityKit.AgentTransactionDemo`（无参）无 `Error`，输出 3 条案例日志 + 1 条摘要日志：
    - `summary total_cases=3 committed=1 rolled_back=2 transaction_mode=all_or_nothing expected_failed_code=E4007 validation=ok`
  - `HCIAbilityKit.AgentTransactionDemo 3 0`：
    - 命中 `committed=true rolled_back=false committed_steps=3`
  - `HCIAbilityKit.AgentTransactionDemo 3 2`：
    - 命中 `committed=false rolled_back=true failed_step_index=2 error_code=E4007 reason=step_failed_all_or_nothing_rollback committed_steps=0`
    - 为预期整单回滚场景，出现 `Warning` 日志但无 `Error`

## 7. 结论

- `Pass`

## 8. 证据

- 构建日志：`C:\Users\50428\AppData\Local\UnrealBuildTool\Log.txt`
- 自动化日志（分离留证）：
  - `Saved/Logs/Automation_AgentExec_E5.log`
  - `Saved/Logs/Automation_AgentTools_E5.log`
  - `Saved/Logs/Automation_AgentDryRun_E5.log`
- 自动化关键证据（助手本地已验证）：
  - `Found 9 automation tests based on 'HCIAbilityKit.Editor.AgentExec'`
  - `Result={成功} Name={TransactionCommitsAllStepsOnSuccess}`
  - `Result={成功} Name={TransactionRollsBackOnFailure}`
  - `Result={成功} Name={TransactionRejectsUnknownToolBeforeExecution}`
  - `Found 3 automation tests based on 'HCIAbilityKit.Editor.AgentTools'`
  - `Found 2 automation tests based on 'HCIAbilityKit.Editor.AgentDryRun'`

## 9. 问题与后续动作

- `StageE-SliceE5` 门禁关闭，进入 `StageE-SliceE6`（SourceControl `Fail-Fast` + 离线本地模式）。
