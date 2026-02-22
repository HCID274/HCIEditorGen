# StageE-SliceE7 本地 Mock 鉴权与本地审计日志（2026-02-22）

- 日期：2026-02-22
- 切片编号：StageE-SliceE7
- 测试人：助手（实现/编译/自动化）+ 用户（UE 手测）
- 当前状态：已完成（用户 UE 手测通过）

## 1. 测试目标

- 落地本地 Mock 鉴权（一期最小口径）：
  - 本地用户名从本地 JSON 配表解析能力；
  - 未命中用户名默认回退 `Guest(read_only)`；
  - `Guest` 执行写工具时拦截并返回 `E4008`；
  - 已授权用户可按能力执行写工具。
- 落地本地审计日志 JSONL：
  - 每次鉴权演示写入 `Saved/HCIAbilityKit/Audit/agent_exec_log.jsonl`；
  - 记录至少包含时间/用户/动作/结果/错误码等核心字段。

## 2. 影响范围

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Agent/HCIAbilityKitAgentExecutionGate.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Agent/HCIAbilityKitAgentExecutionGate.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/HCIAbilityKitEditorModule.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Tests/HCIAbilityKitAgentExecutionGateTests.cpp`
- `SourceData/AbilityKits/Config/agent_rbac_mock.json`
- `Source/HCIEditorGen/文档/00_总进度.md`
- `Source/HCIEditorGen/文档/03_Schema_v1与错误契约.md`
- `AGENTS.md`

## 3. 前置条件

- `StageE-SliceE6` 已通过。
- 工程编译通过。
- UE 编辑器可打开并执行控制台命令。

## 4. 操作步骤

1. 编译插件（助手已完成本地验证）
   - `Build.bat HCIEditorGenEditor Win64 Development -Project=... -WaitMutex -FromMSBuild`
2. 自动化测试（助手已完成本地验证）
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AgentExec; Quit"`（扩展至 17 条）
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AgentTools; Quit"`（回归）
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AgentDryRun; Quit"`（回归）
3. UE 手测（默认三案例摘要 + 自动写本地日志）
   - `HCIAbilityKit.AgentRbacDemo`
4. UE 手测（Guest 回退写操作：应拦截）
   - `HCIAbilityKit.AgentRbacDemo unknown_guest RenameAsset 1`
5. UE 手测（Guest 回退只读：应放行）
   - `HCIAbilityKit.AgentRbacDemo unknown_guest ScanAssets 0`
6. UE 手测（已授权用户写操作：应放行）
   - `HCIAbilityKit.AgentRbacDemo artist_a SetTextureMaxSize 3`
7. （可选）在资源管理器或文本编辑器打开审计日志文件确认行内容
   - `Saved/HCIAbilityKit/Audit/agent_exec_log.jsonl`

## 5. 预期结果

- `HCIAbilityKit.AgentRbacDemo`（无参数）输出 3 条案例日志 + 1 条摘要日志 + 路径日志：
  - 案例字段包含：
    - `request_id`
    - `user`
    - `resolved_role`
    - `user_in_whitelist`
    - `guest_fallback`
    - `tool_name`
    - `capability`
    - `write_like`
    - `asset_count`
    - `allowed`
    - `error_code`
    - `reason`
    - `audit_log_appended`
    - `audit_log_path`
  - 摘要日志包含：
    - `summary total_cases=3`
    - `guest_fallback_cases=2`
    - `audit_log_appends=3`
    - `validation=ok`
  - 路径日志包含：
    - `config_path=...SourceData/AbilityKits/Config/agent_rbac_mock.json`
    - `audit_log_path=...Saved/HCIAbilityKit/Audit/agent_exec_log.jsonl`
- `HCIAbilityKit.AgentRbacDemo unknown_guest RenameAsset 1` 命中：
  - `resolved_role=Guest`
  - `user_in_whitelist=false`
  - `guest_fallback=true`
  - `capability=write`
  - `allowed=false`
  - `error_code=E4008`
  - `reason=guest_read_only_write_blocked`
  - `audit_log_appended=true`
- `HCIAbilityKit.AgentRbacDemo unknown_guest ScanAssets 0` 命中：
  - `resolved_role=Guest`
  - `capability=read_only`
  - `allowed=true`
  - `reason=guest_read_only_allowed`
- `HCIAbilityKit.AgentRbacDemo artist_a SetTextureMaxSize 3` 命中：
  - `resolved_role=Artist`
  - `user_in_whitelist=true`
  - `capability=write`
  - `allowed=true`
  - `reason=rbac_allowed`
- `agent_exec_log.jsonl` 文件存在，且至少新增一行 JSON，包含核心字段：
  - `timestamp_utc`
  - `user`
  - `request_id`
  - `tool_name`
  - `asset_count`
  - `result`
  - `error_code`

## 6. 实际结果（助手本地验证）

- 编译：通过。
  - `Build.bat HCIEditorGenEditor Win64 Development ...` 成功。
- 自动化：通过。
  - `HCIAbilityKit.Editor.AgentExec`：17/17 成功
    - 新增 `MockRbacBlocksGuestWriteFallback`
    - 新增 `MockRbacAllowsGuestReadOnlyFallback`
    - 新增 `MockRbacAllowsConfiguredWriteUser`
    - 新增 `LocalAuditLogJsonLineIncludesCoreFields`
  - `HCIAbilityKit.Editor.AgentTools`：3/3 成功（回归）
  - `HCIAbilityKit.Editor.AgentDryRun`：2/2 成功（回归）
- 说明（日志留证方式）：
  - 使用 `-abslog=` 分离日志文件，避免覆盖 `Saved/Logs/HCIEditorGen.log`。
  - `UnrealEditorServer-HCIEditorGen` 网络噪音日志出现在 UE-Cmd 退出阶段，不作为插件测试失败依据；以 `Result={成功}` 为准。
- UE 手测：通过。
  - `HCIAbilityKit.AgentRbacDemo`（无参）无 `Error`，输出 3 条案例日志 + 摘要日志：
    - `summary total_cases=3 allowed=2 blocked=1 guest_fallback_cases=2 audit_log_appends=3 config_users=3 validation=ok`
  - `HCIAbilityKit.AgentRbacDemo unknown_guest RenameAsset 1`：
    - 命中 `resolved_role=Guest user_in_whitelist=false guest_fallback=true capability=write allowed=false error_code=E4008 reason=guest_read_only_write_blocked audit_log_appended=true`
    - 为预期 Guest 写操作阻断场景，出现 `Warning` 日志但无 `Error`
  - `HCIAbilityKit.AgentRbacDemo unknown_guest ScanAssets 0`：
    - 命中 `resolved_role=Guest capability=read_only allowed=true reason=guest_read_only_allowed`
  - `HCIAbilityKit.AgentRbacDemo artist_a SetTextureMaxSize 3`：
    - 命中 `resolved_role=Artist user_in_whitelist=true capability=write allowed=true reason=rbac_allowed`
  - 用户确认 `Saved/HCIAbilityKit/Audit/agent_exec_log.jsonl` 存在，且包含 `timestamp_utc/user/request_id/tool_name/asset_count/result/error_code` 等核心字段。

## 7. 结论

- `Pass`

## 8. 证据

- 构建日志：`C:\Users\50428\AppData\Local\UnrealBuildTool\Log.txt`
- 自动化日志（分离留证）：
  - `Saved/Logs/Automation_AgentExec_E7.log`
  - `Saved/Logs/Automation_AgentTools_E7.log`
  - `Saved/Logs/Automation_AgentDryRun_E7.log`
- 自动化关键证据（助手本地已验证）：
  - `Found 17 automation tests based on 'HCIAbilityKit.Editor.AgentExec'`
  - `Result={成功} Name={MockRbacBlocksGuestWriteFallback}`
  - `Result={成功} Name={MockRbacAllowsGuestReadOnlyFallback}`
  - `Result={成功} Name={MockRbacAllowsConfiguredWriteUser}`
  - `Result={成功} Name={LocalAuditLogJsonLineIncludesCoreFields}`
  - `Found 3 automation tests based on 'HCIAbilityKit.Editor.AgentTools'`
  - `Found 2 automation tests based on 'HCIAbilityKit.Editor.AgentDryRun'`

## 9. 问题与后续动作

- `StageE-SliceE7` 门禁关闭，进入 `StageE-SliceE8`（规则级安全边界收束：LOD Nanite 拦截 / 类型校验）。
- 已额外完成时间显示兼容调整：对外日志/JSON 时间字符串统一改为北京时间 `+08:00` 输出（字段名暂保持兼容）。
