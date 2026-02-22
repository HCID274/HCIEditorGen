# StageE-SliceE6 SourceControl Fail-Fast 与离线本地模式（2026-02-22）

- 日期：2026-02-22
- 切片编号：StageE-SliceE6
- 测试人：助手（实现/编译/自动化）+ 用户（UE 手测）
- 当前状态：已完成（用户 UE 手测通过）

## 1. 测试目标

- 落地 SourceControl 安全前置（一期最小口径）：
  - `read_only` 工具不触发 `Checkout`；
  - SourceControl 未启用时进入“离线本地模式”，Warning + 放行本地写操作；
  - SourceControl 已启用且 `Checkout` 失败时 `Fail-Fast` 拦截并返回 `E4006`；
  - 不做自动重试与强制解锁。
- 提供 UE 控制台演示入口，验证摘要与单案例日志字段口径。

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

- `StageE-SliceE5` 已通过。
- 工程编译通过。
- UE 编辑器可打开并执行控制台命令。

## 4. 操作步骤

1. 编译插件（助手已完成本地验证）
   - `Build.bat HCIEditorGenEditor Win64 Development -Project=... -WaitMutex -FromMSBuild`
2. 自动化测试（助手已完成本地验证）
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AgentExec; Quit"`（扩展至 13 条）
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AgentTools; Quit"`（回归）
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AgentDryRun; Quit"`（回归）
3. UE 手测（默认三案例摘要）
   - `HCIAbilityKit.AgentSourceControlDemo`
4. UE 手测（离线本地模式：写工具，期望 Warning + 放行）
   - `HCIAbilityKit.AgentSourceControlDemo RenameAsset 0 0`
5. UE 手测（Fail-Fast：启用 SC 且 Checkout 失败，期望拦截）
   - `HCIAbilityKit.AgentSourceControlDemo RenameAsset 1 0`
6. UE 手测（启用 SC 且 Checkout 成功，期望放行）
   - `HCIAbilityKit.AgentSourceControlDemo MoveAsset 1 1`

## 5. 预期结果

- `HCIAbilityKit.AgentSourceControlDemo`（无参数）输出 3 条案例日志 + 1 条摘要日志：
  - 案例字段包含：
    - `request_id`
    - `tool_name`
    - `capability`
    - `write_like`
    - `source_control_enabled`
    - `fail_fast`
    - `offline_local_mode`
    - `checkout_attempted`
    - `checkout_succeeded`
    - `allowed`
    - `error_code`
    - `reason`
  - 摘要日志包含：
    - `summary total_cases=3`
    - `offline_local_mode_cases=1`
    - `fail_fast=true`
    - `expected_blocked_code=E4006`
    - `validation=ok`
- `HCIAbilityKit.AgentSourceControlDemo RenameAsset 0 0` 命中：
  - `capability=write`
  - `source_control_enabled=false`
  - `offline_local_mode=true`
  - `checkout_attempted=false`
  - `allowed=true`
- `HCIAbilityKit.AgentSourceControlDemo RenameAsset 1 0` 命中：
  - `capability=write`
  - `source_control_enabled=true`
  - `checkout_attempted=true`
  - `allowed=false`
  - `error_code=E4006`
  - `reason=source_control_checkout_failed_fail_fast`
- `HCIAbilityKit.AgentSourceControlDemo MoveAsset 1 1` 命中：
  - `capability=write`
  - `source_control_enabled=true`
  - `checkout_attempted=true`
  - `checkout_succeeded=true`
  - `allowed=true`

## 6. 实际结果（助手本地验证）

- 编译：通过。
  - `Build.bat HCIEditorGenEditor Win64 Development ...` 成功。
- 自动化：通过。
  - `HCIAbilityKit.Editor.AgentExec`：13/13 成功
    - 新增 `SourceControlAllowsOfflineLocalModeWhenDisabled`
    - 新增 `SourceControlBlocksEnabledCheckoutFailure`
    - 新增 `SourceControlAllowsReadOnlyWithoutCheckout`
    - 新增 `SourceControlAllowsEnabledCheckoutSuccess`
  - `HCIAbilityKit.Editor.AgentTools`：3/3 成功（回归）
  - `HCIAbilityKit.Editor.AgentDryRun`：2/2 成功（回归）
- 说明（日志留证方式）：
  - 使用 `-abslog=` 分离日志文件，避免覆盖 `Saved/Logs/HCIEditorGen.log`。
  - 包装层偶发 `UnrealEditorServer-HCIEditorGen` 网络报错出现在 UE-Cmd 退出阶段，不属于插件测试失败依据；以自动化测试结果行（`Result={成功}`）为准。
- UE 手测：通过。
  - `HCIAbilityKit.AgentSourceControlDemo`（无参）无 `Error`，输出 3 条案例日志 + 1 条摘要日志：
    - `summary total_cases=3 allowed=2 blocked=1 offline_local_mode_cases=1 fail_fast=true expected_blocked_code=E4006 validation=ok`
  - `HCIAbilityKit.AgentSourceControlDemo RenameAsset 0 0`：
    - 命中 `capability=write source_control_enabled=false offline_local_mode=true checkout_attempted=false allowed=true`
    - 为预期“离线本地模式”场景，出现 `Warning` 日志但无 `Error`
  - `HCIAbilityKit.AgentSourceControlDemo RenameAsset 1 0`：
    - 命中 `capability=write source_control_enabled=true checkout_attempted=true allowed=false error_code=E4006 reason=source_control_checkout_failed_fail_fast`
    - 为预期 `Fail-Fast` 拦截场景，出现 `Warning` 日志但无 `Error`
  - `HCIAbilityKit.AgentSourceControlDemo MoveAsset 1 1`：
    - 命中 `capability=write source_control_enabled=true checkout_attempted=true checkout_succeeded=true allowed=true`

## 7. 结论

- `Pass`

## 8. 证据

- 构建日志：`C:\Users\50428\AppData\Local\UnrealBuildTool\Log.txt`
- 自动化日志（分离留证）：
  - `Saved/Logs/Automation_AgentExec_E6.log`
  - `Saved/Logs/Automation_AgentTools_E6.log`
  - `Saved/Logs/Automation_AgentDryRun_E6.log`
- 自动化关键证据（助手本地已验证）：
  - `Found 13 automation tests based on 'HCIAbilityKit.Editor.AgentExec'`
  - `Result={成功} Name={SourceControlAllowsOfflineLocalModeWhenDisabled}`
  - `Result={成功} Name={SourceControlBlocksEnabledCheckoutFailure}`
  - `Result={成功} Name={SourceControlAllowsReadOnlyWithoutCheckout}`
  - `Result={成功} Name={SourceControlAllowsEnabledCheckoutSuccess}`
  - `Found 3 automation tests based on 'HCIAbilityKit.Editor.AgentTools'`
  - `Found 2 automation tests based on 'HCIAbilityKit.Editor.AgentDryRun'`

## 9. 问题与后续动作

- `StageE-SliceE6` 门禁关闭，进入 `StageE-SliceE7`（本地 Mock 鉴权与本地审计日志）。
