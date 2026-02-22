# StageE-SliceE8 LOD 工具安全边界（Nanite 拦截与类型校验）（2026-02-22）

- 日期：2026-02-22
- 切片编号：StageE-SliceE8
- 测试人：助手（实现/编译/自动化）+ 用户（UE 手测）
- 当前状态：已完成（用户 UE 手测通过）

## 1. 测试目标

- 落地 `SetMeshLODGroup` 的规则级安全边界：
  - 非 `StaticMesh` 目标必须拦截（`E4010`）；
  - `StaticMesh` 且 `Nanite=true` 必须拦截（`E4010`）；
  - 仅 `StaticMesh` 且 `Nanite=false` 允许放行。
- 提供 UE 控制台演示命令，验证日志口径稳定、字段完整。

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

- `StageE-SliceE7` 已通过。
- 工程可编译。
- UE 编辑器可打开并执行控制台命令。

## 4. 操作步骤

1. 编译（助手已完成本地验证）
   - `Build.bat HCIEditorGenEditor Win64 Development -Project=... -WaitMutex -FromMSBuild`
2. 自动化测试（助手已完成本地验证）
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AgentExec; Quit"`（新增 E8 用例）
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AgentTools; Quit"`（回归）
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AgentDryRun; Quit"`（回归）
3. UE 手测（默认三案例摘要）
   - `HCIAbilityKit.AgentLodSafetyDemo`
4. UE 手测（类型非法：应拦截）
   - `HCIAbilityKit.AgentLodSafetyDemo SetMeshLODGroup Texture2D 0`
5. UE 手测（Nanite 网格：应拦截）
   - `HCIAbilityKit.AgentLodSafetyDemo SetMeshLODGroup StaticMesh 1`
6. UE 手测（合法网格：应放行）
   - `HCIAbilityKit.AgentLodSafetyDemo SetMeshLODGroup UStaticMesh 0`

## 5. 预期结果（Pass 判定标准）

1. 上述 4 条命令在合法参数下均无 `Error` 级日志。
2. `HCIAbilityKit.AgentLodSafetyDemo`（无参）输出 3 条案例日志 + 1 条摘要日志，且摘要包含：
   - `summary total_cases=3`
   - `type_blocked=1`
   - `nanite_blocked=1`
   - `expected_blocked_code=E4010`
   - `validation=ok`
3. `HCIAbilityKit.AgentLodSafetyDemo SetMeshLODGroup Texture2D 0` 命中：
   - `capability=write`
   - `is_lod_tool=true`
   - `target_object_class=Texture2D`
   - `is_static_mesh_target=false`
   - `allowed=false`
   - `error_code=E4010`
   - `reason=lod_tool_requires_static_mesh`
4. `HCIAbilityKit.AgentLodSafetyDemo SetMeshLODGroup StaticMesh 1` 命中：
   - `capability=write`
   - `is_lod_tool=true`
   - `target_object_class=StaticMesh`
   - `is_static_mesh_target=true`
   - `nanite_enabled=true`
   - `allowed=false`
   - `error_code=E4010`
   - `reason=lod_tool_nanite_enabled_blocked`
5. `HCIAbilityKit.AgentLodSafetyDemo SetMeshLODGroup UStaticMesh 0` 命中：
   - `capability=write`
   - `is_lod_tool=true`
   - `target_object_class=UStaticMesh`
   - `is_static_mesh_target=true`
   - `nanite_enabled=false`
   - `allowed=true`
   - `reason=lod_tool_static_mesh_non_nanite_allowed`

## 6. 实际结果（助手本地验证）

- 编译：通过
  - `Build.bat HCIEditorGenEditor Win64 Development ...` 成功。
- 自动化：通过
  - `HCIAbilityKit.Editor.AgentExec`：新增 E8 用例通过
    - `LodSafetyBlocksNonStaticMeshTarget`
    - `LodSafetyBlocksNaniteMesh`
    - `LodSafetyAllowsNonNaniteStaticMesh`
  - `HCIAbilityKit.Editor.AgentTools`：3/3 成功（回归）
  - `HCIAbilityKit.Editor.AgentDryRun`：2/2 成功（回归）
- 说明：
  - `UnrealEditor-Cmd` 在当前环境下常无控制台回显，以 `Saved/Logs/HCIEditorGen.log` 中 `Result={成功}` 为准。
  - 一次并行启动两个 `UnrealEditor-Cmd` 进程会产生 `UnrealEditorServer` 网络噪音日志；本次已串行重跑并确认回归结果。
- UE 手测：通过。
  - `HCIAbilityKit.AgentLodSafetyDemo`（无参）无 `Error`，摘要命中：
    - `summary total_cases=3 allowed=1 blocked=2 type_blocked=1 nanite_blocked=1 expected_blocked_code=E4010 validation=ok`
  - `HCIAbilityKit.AgentLodSafetyDemo SetMeshLODGroup Texture2D 0`：
    - 命中 `capability=write is_lod_tool=true target_object_class=Texture2D is_static_mesh_target=false nanite_enabled=false allowed=false error_code=E4010 reason=lod_tool_requires_static_mesh`
  - `HCIAbilityKit.AgentLodSafetyDemo SetMeshLODGroup StaticMesh 1`：
    - 命中 `is_static_mesh_target=true nanite_enabled=true allowed=false error_code=E4010 reason=lod_tool_nanite_enabled_blocked`
  - `HCIAbilityKit.AgentLodSafetyDemo SetMeshLODGroup UStaticMesh 0`：
    - 命中 `is_static_mesh_target=true nanite_enabled=false allowed=true error_code=- reason=lod_tool_static_mesh_non_nanite_allowed`
  - 全部 4 条命令仅出现预期 `Warning/Display`，无 `Error`。

## 7. 结论

- `Pass`

## 8. 证据

- 构建日志：`C:\Users\50428\AppData\Local\UnrealBuildTool\Log.txt`
- 自动化主日志：`Saved/Logs/HCIEditorGen.log`
- 自动化关键证据（助手本地已核对）：
  - `Result={成功} Name={LodSafetyBlocksNonStaticMeshTarget}`
  - `Result={成功} Name={LodSafetyBlocksNaniteMesh}`
  - `Result={成功} Name={LodSafetyAllowsNonNaniteStaticMesh}`
  - `Result={成功} Name={ArgsSchemaFrozen}`（AgentTools 回归）
  - `Result={成功} Name={JsonSerializerIncludesCoreFields}`（AgentDryRun 回归）

## 9. 问题与后续动作

- `StageE-SliceE8` 门禁关闭。
- `Stage E` 安全执行阶段收束完成，主线切换到 `StageF-SliceF1`（自然语言 -> Plan JSON 契约冻结与最小落地）。
