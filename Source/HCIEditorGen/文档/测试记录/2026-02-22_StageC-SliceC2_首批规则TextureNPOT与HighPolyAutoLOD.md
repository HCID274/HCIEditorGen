# StageC-SliceC2 首批规则 TextureNPOT + HighPolyAutoLOD（2026-02-22）

- 日期：2026-02-22
- 切片编号：StageC-SliceC2
- 测试人：助手（实现/编译/自动化）+ 用户（UE 手测）

## 1. 测试目标

- 在 `RuleRegistry` 中落地首批资产规范合规规则：
  - `TextureNPOTRule`
  - `HighPolyAutoLODRule`
- 扫描日志输出规则证据字段（`class/mesh_lods/mesh_nanite/tex_dims`），支持 UE 手测定位。

## 2. 影响范围

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Audit/HCIAbilityKitAuditScanService.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Audit/HCIAbilityKitAuditTagNames.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Audit/HCIAbilityKitAuditScanService.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Audit/HCIAbilityKitAuditRuleRegistry.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/HCIAbilityKitEditorModule.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Tests/HCIAbilityKitAuditRuleRegistryTests.cpp`

## 3. 前置条件

- 工程编译通过。
- 本地可运行自动化测试 `HCIAbilityKit.Editor.AuditRules`。
- 项目内存在至少一个 `UHCIAbilityKitAsset` 且可通过 `RepresentingMesh` 指向 `UStaticMesh`（用于 `HighPolyAutoLODRule` UE 手测观测）。

## 4. 操作步骤

1. 编译插件：
   - `Build.bat HCIEditorGenEditor Win64 Development -Project=... -WaitMutex -FromMSBuild`
2. 自动化测试：
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AuditRules; Quit" -TestExit="Automation Test Queue Empty"`
3. UE 手测（控制台）：
   - `HCIAbilityKit.AuditScan 20`
   - `HCIAbilityKit.AuditScanAsync 1 20`
4. 检查行日志是否包含新增字段：
   - `class=...`
   - `mesh_lods=...`
   - `mesh_nanite=true/false/unknown`
   - `tex_dims=...x...`
5. 若存在“非 Nanite 且高面数且仅 1 个 LOD”的样本，检查是否出现：
   - `first_issue_rule=HighPolyAutoLODRule`
   - 摘要 `issue_warn > 0`

## 5. 预期结果

- 自动化测试发现 4 条 `HCIAbilityKit.Editor.AuditRules.*` 用例并全部通过：
  - `HighPolyAutoLODWarn`
  - `TextureNPOTError`
  - `TriangleExpectedMismatchWarn`
  - `TriangleExpectedMissingShouldNotWarn`
- UE 日志出现新增证据字段。
- `HighPolyAutoLODRule` 在满足条件样本上触发 `Warn`；若所有样本均为 Nanite 或已有多级 LOD，则允许不触发（此时应能从 `mesh_nanite/mesh_lods` 字段解释原因）。

## 6. 实际结果

- 编译：通过。
- 自动化：通过（4/4 成功）。
- UE 手测：通过。
- 用户手测结论（摘要）：
  - 行日志新增字段完整：`class`、`mesh_lods`、`mesh_nanite`、`tex_dims` 均正确输出。
  - `mesh_lods` 与 `mesh_nanite` 数值符合样本实际情况（`TestSkill` 无网格；其余样本为单 LOD 且非 Nanite）。
  - `HighPolyAutoLODRule` 正确触发：3 个高面数、非 Nanite、仅 1 LOD 样本均命中 `audit_issue_count=1`、`first_issue_rule=HighPolyAutoLODRule`、`first_issue_severity=1`。
  - 摘要统计与行日志一致：`issue_assets=3`、`issue_warn=3`、`issue_error=0`。
  - `TextureNPOTRule` 未在 UE 扫描日志自然触发，符合当前扫描对象范围（`UHCIAbilityKitAsset`）的预期；规则本体已由自动化测试覆盖。
  - 同步/异步扫描命令执行完整，异步进度推进正常，无 Error/Warning 异常。

## 7. 结论

- `Pass`

## 8. 证据

- 构建日志：`C:\Users\50428\AppData\Local\UnrealBuildTool\Log.txt`
- 自动化日志：`Saved/Logs/HCIEditorGen.log`
- 自动化关键证据：
  - `Found 4 automation tests based on 'HCIAbilityKit.Editor.AuditRules'`
  - `Result={成功} Name={HighPolyAutoLODWarn}`
  - `Result={成功} Name={TextureNPOTError}`
- UE 手测关键证据（用户回传结论）：
  - 行日志出现：`class=... mesh_lods=... mesh_nanite=... tex_dims=...`
  - 规则命中：`first_issue_rule=HighPolyAutoLODRule`（3 条）
  - 摘要：`issue_assets=3 issue_warn=3 issue_error=0`
- 截图路径：待补
- 录屏路径：待补

## 9. 问题与后续动作

- `StageC-SliceC2` 门禁关闭，进入 `Stage C-SliceC3`（统一审计结果结构与严重级别）。
