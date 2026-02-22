# StageC-SliceC1 RuleRegistry 框架与 Mismatch 规则（2026-02-22）

- 日期：2026-02-22
- 切片编号：StageC-SliceC1
- 测试人：助手（实现/编译/自动化）+ 用户（UE 手测）

## 1. 测试目标

- 落地 Runtime 规则框架：`IHCIAbilityAuditRule + RuleRegistry`。
- 接入默认规则：`TriangleExpectedMismatchRule`（`actual != expected` 触发 `Warn`）。
- 打通 expected 来源：JSON `params.triangle_count_lod0` -> 资产 Tag `hci_triangle_expected_lod0` -> 扫描行字段。
- `AuditScan/AuditScanAsync` 输出规则 issue 统计口径（`issue_assets/info/warn/error`）。

## 2. 影响范围

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Audit/HCIAbilityKitAuditRule.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Audit/HCIAbilityKitAuditRuleRegistry.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Audit/HCIAbilityKitAuditScanService.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Audit/HCIAbilityKitAuditTagNames.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/HCIAbilityKitAsset.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Services/HCIAbilityKitParserService.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Audit/HCIAbilityKitAuditRule.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Audit/HCIAbilityKitAuditRuleRegistry.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Audit/HCIAbilityKitAuditScanService.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/HCIAbilityKitAsset.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Services/HCIAbilityKitParserService.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/HCIAbilityKitEditorModule.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Factories/HCIAbilityKitFactory.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Tests/HCIAbilityKitAuditRuleRegistryTests.cpp`

## 3. 前置条件

- UE 5.4.4 工程可正常编译。
- 本地可执行 `UnrealEditor-Cmd` 自动化测试命令。
- 项目内至少有 1 个 `UHCIAbilityKitAsset` 可用于 `AuditScan` 观测。

## 4. 操作步骤

1. 编译插件：
   - `Build.bat HCIEditorGenEditor Win64 Development -Project=... -WaitMutex -FromMSBuild`
2. 运行自动化测试：
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AuditRules; Quit" -TestExit="Automation Test Queue Empty"`
3. 在 UE 控制台执行（用户手测）：
   - `HCIAbilityKit.AuditScan 20`
   - `HCIAbilityKit.AuditScanAsync 1 20`
4. 观察摘要是否出现：
   - `issue_assets=... issue_info=... issue_warn=... issue_error=...`
5. 观察行日志是否出现：
   - `triangle_expected_lod0=...`
   - `audit_issue_count=... first_issue_rule=TriangleExpectedMismatchRule`

## 5. 预期结果

- 编译通过，无新增编译错误。
- 自动化测试发现并执行 2 条 `HCIAbilityKit.Editor.AuditRules.*`，结果均为成功。
- 若某资产 `triangle_count_lod0_actual != triangle_count_lod0_expected_json`，日志中出现 `TriangleExpectedMismatchRule` 且严重级别为 `Warn`。
- 若 expected 缺失，则不产生 mismatch issue（不误报）。

## 6. 实际结果

- 编译：通过。
- 自动化：通过（2/2 成功）。
- UE 手测：通过。
- 用户手测结论（摘要）：
  - 同步扫描与异步扫描均成功执行，无 Error/Warning 级别异常、崩溃或断言失败。
  - 异步扫描 `batch_size=1` 时进度从 `25%` 推进至 `100%`，`processed=4/4`，分批逻辑正常。
  - 摘要指标合理：`id_coverage=75.0%`、`display_name_coverage=75.0%`、`representing_mesh_coverage=75.0%`、`triangle_tag_coverage=75.0%`、`skipped_locked_or_dirty=0`。
  - `issue_assets=0/issue_info=0/issue_warn=0/issue_error=0` 符合预期（所有测试资产 `triangle_expected_lod0=-1`，规则按设计不误报）。
  - 行日志字段完整：`triangle_expected_lod0=-1`、`audit_issue_count=0`、`first_issue_rule=`（空）。

## 7. 结论

- `Pass`

## 8. 证据

- 构建日志：`C:\Users\50428\AppData\Local\UnrealBuildTool\Log.txt`
- 自动化日志：`Saved/Logs/HCIEditorGen.log`
- 自动化关键证据：
  - `Found 2 automation tests based on 'HCIAbilityKit.Editor.AuditRules'`
  - `Result={成功} Name={TriangleExpectedMismatchWarn}`
  - `Result={成功} Name={TriangleExpectedMissingShouldNotWarn}`
- UE 手测关键证据（用户回传结论）：
  - `HCIAbilityKit.AuditScan 20` 与 `HCIAbilityKit.AuditScanAsync 1 20` 执行成功
  - `issue_assets=0 issue_info=0 issue_warn=0 issue_error=0`
  - 行日志含 `triangle_expected_lod0 / audit_issue_count / first_issue_rule`
- 截图路径：待补
- 录屏路径：待补

## 9. 问题与后续动作

- `StageC-SliceC1` 门禁关闭，进入 `Stage C-SliceC2`（`TextureNPOTRule + HighPolyAutoLODRule`）。
