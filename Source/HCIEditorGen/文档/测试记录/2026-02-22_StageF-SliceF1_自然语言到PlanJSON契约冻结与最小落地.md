# StageF-SliceF1 自然语言 -> Plan JSON 契约冻结与最小落地（2026-02-22）

- 日期：2026-02-22
- 切片编号：StageF-SliceF1
- 测试人：助手（实现/编译/自动化）+ 用户（UE 手测）
- 当前状态：已完成（用户 UE 手测通过）

## 1. 测试目标

- 冻结并落地 `Plan JSON` 最小契约结构（`plan_version/request_id/intent/steps[]`）。
- 提供最小自然语言规划器（关键词路由）将用户意图转换为结构化计划。
- 提供 UE 控制台命令用于验证：
  - 三类一期意图覆盖（命名溯源整理 / 关卡排雷 / 资产规范合规）
  - 步骤字段完整性（`tool_name/args/risk_level/requires_confirm/rollback_strategy`）
  - JSON 序列化输出字段完整性

## 2. 影响范围

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Agent/HCIAbilityKitAgentPlan.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Agent/HCIAbilityKitAgentPlan.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Agent/HCIAbilityKitAgentPlanJsonSerializer.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Agent/HCIAbilityKitAgentPlanJsonSerializer.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Agent/HCIAbilityKitAgentPlanner.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Agent/HCIAbilityKitAgentPlanner.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/HCIAbilityKitEditorModule.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Tests/HCIAbilityKitAgentPlanTests.cpp`
- `Source/HCIEditorGen/文档/00_总进度.md`
- `Source/HCIEditorGen/文档/03_Schema_v1与错误契约.md`
- `Source/HCIEditorGen/文档/04_代码框架总览_树状图.md`
- `AGENTS.md`

## 3. 前置条件

- `StageE-SliceE8` 已通过。
- 工程可编译。
- UE 编辑器可打开并执行控制台命令。

## 4. 操作步骤

1. 编译（助手已完成本地验证）
   - `Build.bat HCIEditorGenEditor Win64 Development -Project=... -WaitMutex -FromMSBuild`
2. 自动化测试（助手已完成本地验证）
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AgentPlan; Quit"`（新增 F1 用例）
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AgentTools; Quit"`（回归）
3. UE 手测（默认三案例摘要 + 步骤列表）
   - `HCIAbilityKit.AgentPlanDemo`
4. UE 手测（命名归档意图，自定义输入）
   - `HCIAbilityKit.AgentPlanDemo 整理 临时目录 资产 按 规范 命名 并 归档`
5. UE 手测（关卡排雷意图，自定义输入）
   - `HCIAbilityKit.AgentPlanDemo 检查 当前 关卡 选中 物体 的 碰撞 和 默认材质`
6. UE 手测（Plan JSON 契约输出）
   - `HCIAbilityKit.AgentPlanDemoJson 整理 临时目录 资产`

## 5. 预期结果（Pass 判定标准）

1. 上述 4 条命令（步骤 3~6）在合法参数下均无 `Error` 级日志。
2. `HCIAbilityKit.AgentPlanDemo`（无参）输出 3 条案例摘要 + 1 条总摘要，且总摘要包含：
   - `summary total_cases=3`
   - `supports_intents=naming_traceability|level_risk|asset_compliance`
   - `validation=ok`
3. 默认案例中至少覆盖三类意图摘要（`case=... summary`）：
   - 命名归档案例（`intent=normalize_temp_assets_by_metadata`）
   - 关卡排雷案例（`intent=scan_level_mesh_risks`）
   - 资产合规案例（`intent=batch_fix_asset_compliance`）
4. `HCIAbilityKit.AgentPlanDemo 整理 ...` 输出命中：
   - `case=custom summary`
   - `intent=normalize_temp_assets_by_metadata`
   - `route_reason=naming_traceability_temp_assets`
   - `validation=ok`
   - 至少一条 `row=` 日志包含：
     - `tool_name=NormalizeAssetNamingByMetadata`
     - `risk_level=write`
     - `requires_confirm=true`
     - `rollback_strategy=all_or_nothing`
     - `args=` 中包含 `metadata_source":"auto"`
     - `args=` 中包含 `prefix_mode":"auto_by_asset_class"`
     - `args=` 中包含 `target_root":"/Game/`
5. `HCIAbilityKit.AgentPlanDemo 检查 ... 碰撞 ... 默认材质` 输出命中：
   - `intent=scan_level_mesh_risks`
   - `route_reason=level_risk_collision_material`
   - 至少一条 `row=` 日志包含：
     - `tool_name=ScanLevelMeshRisks`
     - `risk_level=read_only`
     - `requires_confirm=false`
6. `HCIAbilityKit.AgentPlanDemoJson 整理 临时目录 资产` 输出 JSON 字符串，且包含：
   - `"plan_version"`
   - `"request_id"`
   - `"intent"`
   - `"steps"`
   - `"tool_name"`
   - `"args"`
   - `"risk_level"`
   - `"requires_confirm"`
   - `"rollback_strategy"`
   - `"expected_evidence"`

## 6. 实际结果（助手本地验证）

- 编译：通过
  - `Build.bat HCIEditorGenEditor Win64 Development ...` 成功。
- 自动化：通过
  - `HCIAbilityKit.Editor.AgentPlan`：3/3 成功
    - `PlannerSupportsNamingArchiveIntent`
    - `PlannerSupportsLevelRiskIntent`
    - `JsonSerializerIncludesCoreContractFields`
  - `HCIAbilityKit.Editor.AgentTools`：3/3 成功（回归）
- 说明：
  - `UnrealEditor-Cmd` 在当前环境下常无控制台回显，以 `Saved/Logs/HCIEditorGen.log` 中 `Result={成功}` 为准。
  - 并行启动两个 `UnrealEditor-Cmd` 进程会争抢日志文件，本次 `AgentTools` 已串行重跑留证。
- UE 手测：通过。
  - 4 条命令（`AgentPlanDemo` 无参、命名归档意图、关卡排雷意图、`AgentPlanDemoJson`）均无 `Error/Warning`。
  - 无参摘要命中：
    - `summary total_cases=3 built=3 validation_ok=3 plan_version=1 supports_intents=naming_traceability|level_risk|asset_compliance validation=ok`
  - 命名归档意图命中：
    - `intent=normalize_temp_assets_by_metadata`
    - `route_reason=naming_traceability_temp_assets`
    - `tool_name=NormalizeAssetNamingByMetadata`
    - `risk_level=write`
  - 关卡排雷意图命中：
    - `intent=scan_level_mesh_risks`
    - `tool_name=ScanLevelMeshRisks`
    - `risk_level=read_only`
  - `AgentPlanDemoJson` 输出 JSON，覆盖：
    - 顶层：`plan_version/request_id/intent/steps`
    - 步骤：`tool_name/args/risk_level/requires_confirm/rollback_strategy/expected_evidence`

## 7. 结论

- `Pass`

## 8. 证据

- 构建日志：`C:\Users\50428\AppData\Local\UnrealBuildTool\Log.txt`
- 自动化主日志：`Saved/Logs/HCIEditorGen.log`
- 自动化关键证据（助手本地已核对）：
  - `Result={成功} Name={JsonSerializerIncludesCoreContractFields}`
  - `Result={成功} Name={PlannerSupportsLevelRiskIntent}`
  - `Result={成功} Name={PlannerSupportsNamingArchiveIntent}`
  - `Result={成功} Name={ArgsSchemaFrozen}`（AgentTools 回归）
  - `Result={成功} Name={DomainCoverageAndFlags}`（AgentTools 回归）
  - `Result={成功} Name={RegistryWhitelistFrozen}`（AgentTools 回归）

## 9. 问题与后续动作

- `StageF-SliceF1` 门禁关闭。
- 主线切换到 `StageF-SliceF2`（Plan 校验器：参数完整性/权限/风险/阈值）。
