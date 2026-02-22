# StageE-SliceE2 Dry-Run Diff 契约与 UE 预览（2026-02-22）

- 日期：2026-02-22
- 切片编号：StageE-SliceE2
- 测试人：助手（实现/编译/自动化）+ 用户（UE 手测）
- 当前状态：已完成（用户 UE 手测通过）

## 1. 测试目标

- 落地 Dry-Run Diff 契约最小结构（`request_id/summary/diff_items[]`）。
- 在 UE 中提供可见的 Diff 列表预览与定位入口（E2 最小预览链路）：
  - Actor 行走 `Camera Focus`
  - Asset 行走 `SyncBrowserToObjects`
- 为后续 `E3~E7` 的“确认/事务/SC/鉴权”执行链打好统一前置数据结构。

## 2. 影响范围

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Agent/HCIAbilityKitDryRunDiff.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Agent/HCIAbilityKitDryRunDiff.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Agent/HCIAbilityKitDryRunDiffJsonSerializer.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Agent/HCIAbilityKitDryRunDiffJsonSerializer.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/HCIAbilityKitEditorModule.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Tests/HCIAbilityKitDryRunDiffTests.cpp`
- `Source/HCIEditorGen/文档/00_总进度.md`
- `Source/HCIEditorGen/文档/03_Schema_v1与错误契约.md`
- `Source/HCIEditorGen/文档/04_代码框架总览_树状图.md`
- `AGENTS.md`

## 3. 前置条件

- `StageE-SliceE1` 已通过。
- 工程编译通过。
- UE 编辑器可打开并执行控制台命令。

## 4. 操作步骤

1. 编译插件（助手已完成本地验证）：
   - `Build.bat HCIEditorGenEditor Win64 Development -Project=... -WaitMutex -FromMSBuild`
2. 自动化测试（助手已完成本地验证）：
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AgentDryRun; Quit"`
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AgentTools; Quit"`（回归）
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AuditResults; Quit"`（回归）
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AuditScanAsync; Quit"`（回归）
3. UE 手测：生成 Dry-Run Diff 列表预览
   - `HCIAbilityKit.DryRunDiffPreviewDemo`
4. UE 手测：查看 JSON 契约输出（日志）
   - `HCIAbilityKit.DryRunDiffPreviewJson`
5. UE 手测：测试定位入口（至少测 1 条 Asset 行）
   - `HCIAbilityKit.DryRunDiffLocate 0`
6. UE 手测：测试 Actor 行定位策略日志（可接受找不到 Actor，但策略必须是 `camera_focus`）
   - `HCIAbilityKit.DryRunDiffLocate 2`

## 5. 预期结果

- `DryRunDiffPreviewDemo` 输出摘要日志：
  - `[HCIAbilityKit][DryRunDiff] summary request_id=... total_candidates=... modifiable=... skipped=...`
- `row=` 列表日志包含核心字段：
  - `asset_path`
  - `field`
  - `before`
  - `after`
  - `tool_name`
  - `risk`
  - `skip_reason`
- `row=` 列表日志包含扩展字段：
  - `object_type`
  - `locate_strategy`
  - `evidence_key`
- 至少一条 `object_type=asset` 行满足：
  - `locate_strategy=sync_browser`
- 至少一条 `object_type=actor` 行满足：
  - `locate_strategy=camera_focus`
- `DryRunDiffPreviewJson` 输出 JSON 日志，能看到：
  - `request_id`
  - `summary`
  - `diff_items`
  - `locate_strategy`
  - `evidence_key`
- `DryRunDiffLocate 0` 输出定位结果日志：
  - `[HCIAbilityKit][DryRunDiff] locate row=0 ... strategy=sync_browser ...`
- `DryRunDiffLocate 2` 输出定位结果日志：
  - `[HCIAbilityKit][DryRunDiff] locate row=2 ... strategy=camera_focus ...`
  - 即使 `actor_not_found` 也可接受（样例路径可能不在当前关卡），但策略必须正确

## 6. 实际结果

- 编译：通过（助手本地已验证）。
- 自动化：通过（助手本地已验证）。
  - `HCIAbilityKit.Editor.AgentDryRun`：2/2 成功（`NormalizeLocateStrategy / JsonSerializerIncludesCoreFields`）。
  - `HCIAbilityKit.Editor.AgentTools`：3/3 成功（回归）。
  - `HCIAbilityKit.Editor.AuditResults`：3/3 成功（回归）。
  - `HCIAbilityKit.Editor.AuditScanAsync`：5/5 成功（回归）。
- 本地命令行 smoke（说明）：
  - `UnrealEditor-Cmd -ExecCmds="HCIAbilityKit.DryRunDiffPreviewDemo; ..."` 仅稳定记录 `Cmd:`，未稳定执行插件控制台命令日志，判定为 `ExecCmds` 时机限制；不作为 E2 失败依据。
- UE 手测：通过。
  - `HCIAbilityKit.DryRunDiffPreviewDemo` 无 `Error`，日志仅含正常 `Display` 输出。
  - 摘要日志命中：
    - `summary request_id=req_dryrun_demo_... total_candidates=4 modifiable=3 skipped=1`
  - `row=` 列表日志字段完整，包含：
    - 基础字段：`asset_path/field/before/after/tool_name/risk/skip_reason`
    - 扩展字段：`object_type/locate_strategy/evidence_key`
  - `asset` 行定位策略正确：
    - `row0/row1/row3` 均为 `object_type=asset locate_strategy=sync_browser`
  - `actor` 行定位策略正确：
    - `row2` 为 `object_type=actor locate_strategy=camera_focus`
  - `HCIAbilityKit.DryRunDiffPreviewJson` 输出中包含：
    - `request_id`
    - `summary`
    - `diff_items`
    - `locate_strategy`
    - `evidence_key`
  - `HCIAbilityKit.DryRunDiffLocate 0` 输出 `strategy=sync_browser`，且定位成功。
  - `HCIAbilityKit.DryRunDiffLocate 2` 输出 `strategy=camera_focus`；`actor_not_found` 为允许结果（样例 Actor 不在当前关卡）。

## 7. 结论

- `Pass`

## 8. 证据

- 构建日志：`C:\Users\50428\AppData\Local\UnrealBuildTool\Log.txt`
- 自动化日志：`Saved/Logs/HCIEditorGen.log`
- 自动化关键证据（助手本地已验证）：
  - `Found 2 automation tests based on 'HCIAbilityKit.Editor.AgentDryRun'`
  - `Result={成功} Name={JsonSerializerIncludesCoreFields}`
  - `Result={成功} Name={NormalizeLocateStrategy}`
  - `Found 3 automation tests based on 'HCIAbilityKit.Editor.AgentTools'`
  - `Found 3 automation tests based on 'HCIAbilityKit.Editor.AuditResults'`
  - `Found 5 automation tests based on 'HCIAbilityKit.Editor.AuditScanAsync'`
- UE 手测关键证据（用户回传结论）：
  - `DryRunDiffPreviewDemo` 无 `Error`，摘要日志命中 `total_candidates=4 modifiable=3 skipped=1`
  - `row=` 列表日志字段完整（基础字段 + `object_type/locate_strategy/evidence_key`）
  - 存在 `object_type=asset locate_strategy=sync_browser` 行与 `object_type=actor locate_strategy=camera_focus` 行
  - `DryRunDiffPreviewJson` 含 `request_id/summary/diff_items/locate_strategy/evidence_key`
  - `DryRunDiffLocate 0 -> strategy=sync_browser`；`DryRunDiffLocate 2 -> strategy=camera_focus`（`actor_not_found` 可接受）

## 9. 问题与后续动作

- `StageE-SliceE2` 门禁关闭，进入 `StageE-SliceE3`（确认门禁 + 事务/SourceControl 安全前置）。
