# StageC-SliceC4 JSON 报告导出（2026-02-22）

- 日期：2026-02-22
- 切片编号：StageC-SliceC4
- 测试人：助手（实现/编译/自动化/命令行 smoke）+ 用户（UE 手测）

## 1. 测试目标

- 基于 `StageC-SliceC3` 的统一审计结果结构，完成 JSON 报告导出闭环。
- 输出结果必须包含：
  - `rule_id / severity / reason / hint`
  - `triangle_source`
  - `evidence`（规则证据键值对）
- Editor 提供可直接执行的导出命令：
  - `HCIAbilityKit.AuditExportJson [output_json_path]`

## 2. 影响范围

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Audit/HCIAbilityKitAuditReportJsonSerializer.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Audit/HCIAbilityKitAuditReportJsonSerializer.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/HCIAbilityKitEditorModule.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Tests/HCIAbilityKitAuditRuleRegistryTests.cpp`

## 3. 前置条件

- `StageC-SliceC3` 已通过（统一结果结构可用）。
- 工程编译通过。
- 可运行自动化测试：
  - `HCIAbilityKit.Editor.AuditResults`
  - `HCIAbilityKit.Editor.AuditScanAsync`（回归）
- 项目内存在可触发规则的 `UHCIAbilityKitAsset` 样本（便于导出结果中出现 `results[]`）。

## 4. 操作步骤

1. 编译插件：
   - `Build.bat HCIEditorGenEditor Win64 Development -Project=... -WaitMutex -FromMSBuild`
2. 自动化测试：
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AuditResults; Quit"`
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AuditScanAsync; Quit"`
3. UE 手测（控制台）：
   - `HCIAbilityKit.AuditExportJson`
   - 或指定路径：`HCIAbilityKit.AuditExportJson Saved/HCIAbilityKit/AuditReports/c4_user_verify.json`
4. 记录日志中的成功行：
   - `[HCIAbilityKit][AuditExportJson] success path=... run_id=... results=...`
5. 打开导出文件并检查至少一条 `results[]`：
   - 包含 `rule_id`
   - 包含 `severity`
   - 包含 `triangle_source`
   - 包含 `evidence` 对象

## 5. 预期结果

- 自动化测试通过：
  - `HCIAbilityKit.Editor.AuditResults.BuildReportFlattensIssues`
  - `HCIAbilityKit.Editor.AuditResults.JsonSerializerIncludesCoreTraceFields`
  - `HCIAbilityKit.Editor.AuditResults.SeverityStringMapping`
  - `HCIAbilityKit.Editor.AuditScanAsync.*`（回归）
- 导出命令成功落盘 JSON 文件。
- JSON 报告中 `results[]` 项包含 `triangle_source` 和 `evidence`。

## 6. 实际结果

- 编译：通过。
- 自动化：通过。
  - `AuditResults`：3/3 成功（新增 `JsonSerializerIncludesCoreTraceFields`）。
  - `AuditScanAsync`：2/2 成功（回归）。
- 命令行 smoke：通过。
  - 执行 `HCIAbilityKit.AuditExportJson Saved/HCIAbilityKit/AuditReports/c4_local_smoke.json`
  - 日志输出 `success path=... run_id=... results=3 issue_assets=3`
  - 导出文件存在并确认包含 `triangle_source/evidence`
- UE 手测：通过。
- 用户手测结论（摘要）：
  - `HCIAbilityKit.AuditExportJson` 无参数执行与指定路径执行均成功，无 `Error` 级日志。
  - 两次成功日志均包含 `path / run_id / results`（并额外包含 `issue_assets / source`）。
  - 结合本地 smoke 与用户回传结论，确认导出 JSON 文件存在且 `results[]` 包含 `triangle_source` 与 `evidence`。

## 7. 结论

- `Pass`

## 8. 证据

- 构建日志：`C:\Users\50428\AppData\Local\UnrealBuildTool\Log.txt`
- 自动化日志：`Saved/Logs/HCIEditorGen.log`
- 本地 smoke 导出文件：`Saved/HCIAbilityKit/AuditReports/c4_local_smoke.json`
- 自动化关键证据：
  - `Found 3 automation tests based on 'HCIAbilityKit.Editor.AuditResults'`
  - `Result={成功} Name={JsonSerializerIncludesCoreTraceFields}`
  - `Found 2 automation tests based on 'HCIAbilityKit.Editor.AuditScanAsync'`
- 导出命令关键证据：
  - `[HCIAbilityKit][AuditExportJson] success path=.../c4_local_smoke.json run_id=... results=3 issue_assets=3 source=asset_registry_fassetdata`
  - 导出内容命中：`\"triangle_source\":\"tag_cached\"`、`\"evidence\":{`
- UE 手测关键证据（用户回传结论）：
  - 无参数执行：`HCIAbilityKit.AuditExportJson` 成功，无 Error
  - 指定路径执行：`HCIAbilityKit.AuditExportJson Saved/HCIAbilityKit/AuditReports/c4_user_verify.json` 成功，无 Error
  - 成功日志包含 `path/run_id/results`
  - JSON 文件存在且 `results[]` 包含 `triangle_source/evidence`

## 9. 问题与后续动作

- `StageC-SliceC4` 门禁关闭，进入 `Stage D-SliceD1`（深度检查批次策略与分批加载释放）。
