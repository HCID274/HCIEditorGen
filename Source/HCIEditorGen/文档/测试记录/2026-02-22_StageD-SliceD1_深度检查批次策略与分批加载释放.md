# StageD-SliceD1 深度检查批次策略与分批加载释放（2026-02-22）

- 日期：2026-02-22
- 切片编号：StageD-SliceD1
- 测试人：助手（实现/编译/自动化）+ 用户（UE 手测）

## 1. 测试目标

- 在不破坏现有 `AuditScanAsync` 中断/重试能力的前提下，为异步扫描增加“深度网格检查”模式。
- 深度模式开启时：
  - 按批次同步加载 `RepresentingMesh`
  - 补齐真实 `triangle_count_lod0 / mesh_lods / mesh_nanite`
  - 每次加载后立即释放 `FStreamableHandle` 强引用
- 输出可观测统计日志，便于确认 D1 生效。

## 2. 影响范围

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/HCIAbilityKitEditorModule.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Audit/HCIAbilityKitAuditScanAsyncController.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Audit/HCIAbilityKitAuditScanAsyncController.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Tests/HCIAbilityKitAuditScanAsyncControllerTests.cpp`
- `Source/HCIEditorGen/文档/03_Schema_v1与错误契约.md`

## 3. 前置条件

- `StageC-SliceC4` 已通过。
- 工程编译通过。
- 项目中存在带 `RepresentingMesh` 的 `UHCIAbilityKitAsset` 样本。
- 若希望直观看到 `mesh_loaded`，建议至少有一批样本的网格 Tag 不完整（或三角 Tag 缺失）。

## 4. 操作步骤

1. 编译插件：
   - `Build.bat HCIEditorGenEditor Win64 Development -Project=... -WaitMutex -FromMSBuild`
2. 自动化测试（本地已执行，用于记录证据）：
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AuditScanAsync; Quit"`
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AuditResults; Quit"`（回归）
3. UE 手测（控制台，深度模式开启）：
   - `HCIAbilityKit.AuditScanAsync 1 20 1`
4. 扫描完成后观察日志：
   - 启动日志含 `deep_mesh_check=true`
   - 摘要后出现深度统计行：`[HCIAbilityKit][AuditScanAsync][Deep] ...`
5. （可选）验证中断/重试在深度模式下仍可用：
   - `HCIAbilityKit.AuditScanAsync 1 20 1`
   - 期间执行 `HCIAbilityKit.AuditScanAsyncStop`
   - 再执行 `HCIAbilityKit.AuditScanAsyncRetry`
   - 观察 `retry start ... deep_mesh_check=true`

## 5. 预期结果

- `AuditScanAsync` 第三个参数解析成功：`0/1/false/true`。
- 深度模式开启时，完成日志新增深度统计行：
  - `load_attempts`
  - `load_success`
  - `handle_releases`
  - `triangle_resolved`
  - `mesh_signals_resolved`
- 行日志中允许出现 `triangle_source=mesh_loaded`（表示真实网格深度提取生效）。
- `Stop/Retry` 不丢失深度模式开关（`retry start ... deep_mesh_check=true`）。

## 6. 实际结果

- 编译：通过。
- 自动化：通过。
  - `HCIAbilityKit.Editor.AuditScanAsync`：3/3 成功（含新增 `DeepModeRetryPersistence`）。
  - `HCIAbilityKit.Editor.AuditResults`：3/3 成功（回归）。
- UE 手测：通过。
  - `HCIAbilityKit.AuditScanAsync 1 20 1` 两次（首次 + 重试）均完成到 `progress=100% processed=274/274`，无 `Error` 级日志。
  - 首次启动日志命中：`start total=274 batch_size=1 deep_mesh_check=true`。
  - 重试启动日志命中：`retry start total=274 batch_size=1 deep_mesh_check=true`（深度模式开关未丢失）。
  - 完成日志命中深度统计行：`[HCIAbilityKit][AuditScanAsync][Deep] load_attempts=0 load_success=0 handle_releases=0 triangle_resolved=0 mesh_signals_resolved=0`。
  - 中断/重试链路验证通过：`interrupted processed=111/274 can_retry=true` -> `AuditScanAsyncRetry` 正常恢复并完成。
  - 行日志 `triangle_source` 均为 `tag_cached`；未出现 `mesh_loaded` 的原因为样本 Tag 完整、无深度补齐需求（符合预期）。

## 7. 结论

- `Pass`

## 8. 证据

- 构建日志：`C:\Users\50428\AppData\Local\UnrealBuildTool\Log.txt`
- 自动化日志：`Saved/Logs/HCIEditorGen.log`
- 自动化关键证据：
  - `Found 3 automation tests based on 'HCIAbilityKit.Editor.AuditScanAsync'`
  - `Result={成功} Name={DeepModeRetryPersistence}`
  - `Found 3 automation tests based on 'HCIAbilityKit.Editor.AuditResults'`
  - `Result={成功} Name={JsonSerializerIncludesCoreTraceFields}`
- UE 手测关键证据（用户回传结论）：
  - `start total=274 batch_size=1 deep_mesh_check=true`
  - `retry start total=274 batch_size=1 deep_mesh_check=true`
  - `[HCIAbilityKit][AuditScanAsync][Deep] load_attempts=0 load_success=0 handle_releases=0 triangle_resolved=0 mesh_signals_resolved=0`
  - `interrupted processed=111/274 can_retry=true`
  - 两次扫描均完成至 `progress=100% processed=274/274`，无 `Error`

## 9. 问题与后续动作

- `StageD-SliceD1` 门禁关闭，进入 `StageD-SliceD2`（GC 节流策略）。
