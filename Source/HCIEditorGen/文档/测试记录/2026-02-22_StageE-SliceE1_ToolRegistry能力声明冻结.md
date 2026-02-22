# StageE-SliceE1 Tool Registry 能力声明冻结（2026-02-22）

- 日期：2026-02-22
- 切片编号：StageE-SliceE1
- 测试人：助手（实现/编译/自动化）+ 用户（UE 手测）
- 当前状态：已完成（用户 UE 手测通过）

## 1. 测试目标

- 冻结一期 `Tool Registry` 能力声明（白名单 + `args_schema` + 能力元信息）。
- 确认三维业务工具覆盖已落地：
  - `AssetCompliance`
  - `LevelRisk`
  - `NamingTraceability`
- 提供 UE 侧可见证据（控制台命令日志），用于后续 `Stage E/F` Executor 接入前的契约验收。

## 2. 影响范围

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Agent/HCIAbilityKitToolRegistry.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Agent/HCIAbilityKitToolRegistry.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/HCIAbilityKitEditorModule.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Tests/HCIAbilityKitToolRegistryTests.cpp`
- `Source/HCIEditorGen/文档/00_总进度.md`
- `Source/HCIEditorGen/文档/03_Schema_v1与错误契约.md`
- `Source/HCIEditorGen/文档/04_代码框架总览_树状图.md`
- `AGENTS.md`

## 3. 前置条件

- `StageD-SliceD3` 已通过。
- 工程编译通过。
- UE 编辑器可打开并可执行控制台命令。

## 4. 操作步骤

1. 编译插件（助手已完成本地验证）：
   - `Build.bat HCIEditorGenEditor Win64 Development -Project=... -WaitMutex -FromMSBuild`
2. 自动化测试（助手已完成本地验证）：
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AgentTools; Quit"`
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AuditScanAsync; Quit"`（回归）
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AuditResults; Quit"`（回归）
3. UE 手测：输出全量 Tool Registry 声明
   - `HCIAbilityKit.ToolRegistryDump`
4. UE 手测：输出单工具（建议检查 LOD 工具的枚举冻结）
   - `HCIAbilityKit.ToolRegistryDump SetMeshLODGroup`
5. 检查日志：
   - 是否出现 `tool=` 行（能力元信息）
   - 是否出现 `arg tool=...` 行（参数约束）
   - 是否出现 `summary total_registered=... validation=ok`

## 5. 预期结果

- 命令执行成功，无 `Error`。
- `summary` 日志至少包含：
  - `total_registered=7`
  - `validation=ok`
- 全量输出中至少出现以下工具：
  - `ScanAssets`
  - `SetTextureMaxSize`
  - `SetMeshLODGroup`
  - `ScanLevelMeshRisks`
  - `NormalizeAssetNamingByMetadata`
  - `RenameAsset`
  - `MoveAsset`
- `SetMeshLODGroup` 的 `lod_group` 参数日志包含冻结枚举：
  - `LevelArchitecture|SmallProp|LargeProp|Foliage|Character`
- `SetTextureMaxSize` 的 `max_size` 参数日志包含冻结枚举：
  - `256|512|1024|2048|4096|8192`
- `ScanLevelMeshRisks` 的日志体现：
  - `scope` 枚举（`selected|all`）
  - `checks` 枚举（`missing_collision|default_material`）
  - `max_actor_count` 范围（`1..5000`）

## 6. 实际结果

- 编译：通过（助手本地已验证）。
- 自动化：通过（助手本地已验证）。
  - `HCIAbilityKit.Editor.AgentTools`：3/3 成功（`RegistryWhitelistFrozen / ArgsSchemaFrozen / DomainCoverageAndFlags`）。
  - `HCIAbilityKit.Editor.AuditScanAsync`：5/5 成功（回归）。
  - `HCIAbilityKit.Editor.AuditResults`：3/3 成功（回归）。
- UE 手测：通过。
  - `HCIAbilityKit.ToolRegistryDump` 与 `HCIAbilityKit.ToolRegistryDump SetMeshLODGroup` 执行成功，全程无 `Error/Warning`。
  - 全量汇总行：`summary total_registered=7 printed=7 filter=- validation=ok`。
  - 单工具汇总行：`summary total_registered=7 printed=1 filter=SetMeshLODGroup validation=ok`。
  - 全量 `tool=` 行覆盖 7 个白名单工具：
    - `MoveAsset`
    - `NormalizeAssetNamingByMetadata`
    - `RenameAsset`
    - `ScanAssets`
    - `ScanLevelMeshRisks`
    - `SetMeshLODGroup`
    - `SetTextureMaxSize`
  - `SetMeshLODGroup.lod_group` 参数枚举日志包含：
    - `LevelArchitecture|SmallProp|LargeProp|Foliage|Character`
  - `SetTextureMaxSize.max_size` 参数枚举日志包含：
    - `256|512|1024|2048|4096|8192`
  - `ScanLevelMeshRisks` 参数约束日志包含：
    - `scope: selected|all`
    - `checks: missing_collision|default_material`
    - `max_actor_count: 1..5000`

## 7. 结论

- `Pass`

## 8. 证据

- 构建日志：`C:\Users\50428\AppData\Local\UnrealBuildTool\Log.txt`
- 自动化日志：`Saved/Logs/HCIEditorGen.log`
- 自动化关键证据（助手本地已验证）：
  - `Found 3 automation tests based on 'HCIAbilityKit.Editor.AgentTools'`
  - `Result={成功} Name={ArgsSchemaFrozen}`
  - `Result={成功} Name={DomainCoverageAndFlags}`
  - `Result={成功} Name={RegistryWhitelistFrozen}`
  - `Found 5 automation tests based on 'HCIAbilityKit.Editor.AuditScanAsync'`
  - `Found 3 automation tests based on 'HCIAbilityKit.Editor.AuditResults'`
- UE 手测关键证据（用户回传结论）：
  - 无 `Error/Warning`，命令执行成功
  - `summary total_registered=7 printed=7 filter=- validation=ok`
  - `summary total_registered=7 printed=1 filter=SetMeshLODGroup validation=ok`
  - `SetMeshLODGroup` 参数行包含 `str_enum=LevelArchitecture|SmallProp|LargeProp|Foliage|Character`
  - `SetTextureMaxSize` 参数行包含 `int_enum=256|512|1024|2048|4096|8192`
  - `ScanLevelMeshRisks` 参数行包含 `scope/checks` 枚举和 `max_actor_count int_range=1..5000`

## 9. 问题与后续动作

- `StageE-SliceE1` 门禁关闭，进入 `StageE-SliceE2`（Dry-Run Diff 契约与 UE 面板展示）。
