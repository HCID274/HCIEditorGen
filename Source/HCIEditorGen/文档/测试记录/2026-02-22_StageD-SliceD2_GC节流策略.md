# StageD-SliceD2 GC 节流策略（2026-02-22）

- 日期：2026-02-22
- 切片编号：StageD-SliceD2
- 测试人：助手（实现/编译/自动化）+ 用户（UE 手测）

## 1. 测试目标

- 在 `StageD-SliceD1` 深度扫描基础上加入 GC 节流策略：
  - 每 `N` 批调用一次 `CollectGarbage`
  - 记录批次耗时与内存峰值
  - 统计 `gc_runs`
- 确保 `Stop/Retry` 不丢失 `gc_every_n_batches` 配置。

## 2. 影响范围

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/HCIAbilityKitEditorModule.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Audit/HCIAbilityKitAuditScanAsyncController.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Audit/HCIAbilityKitAuditScanAsyncController.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Tests/HCIAbilityKitAuditScanAsyncControllerTests.cpp`
- `Source/HCIEditorGen/文档/00_总进度.md`
- `Source/HCIEditorGen/文档/03_Schema_v1与错误契约.md`

## 3. 前置条件

- `StageD-SliceD1` 已通过。
- 工程编译通过。
- 存在可运行 `HCIAbilityKit.AuditScanAsync` 的 `UHCIAbilityKitAsset` 样本。
- 建议样本数量较多（便于命中多次批次与 GC 节流日志）。

## 4. 操作步骤

1. 编译插件：
   - `Build.bat HCIEditorGenEditor Win64 Development -Project=... -WaitMutex -FromMSBuild`
2. 自动化测试（本地已执行）：
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AuditScanAsync; Quit"`
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AuditResults; Quit"`（回归）
3. UE 手测（建议先降帧方便观察）：
   - `t.MaxFPS = "5"`
4. 运行深度扫描 + 高频 GC（便于快速验证）：
   - `HCIAbilityKit.AuditScanAsync 1 20 1 1`
5. 扫描完成后检查日志：
   - 启动日志包含 `deep_mesh_check=true gc_every_n_batches=1`
   - 完成后存在深度统计行 `[HCIAbilityKit][AuditScanAsync][Deep] ...`
   - 深度统计行中：
     - `gc_every_n_batches=1`
     - `gc_runs > 0`
     - `max_batch_ms=...`
     - `peak_used_physical_mib=...`
6. 验证中断/重试保留 GC 配置（可选但建议）：
   - `HCIAbilityKit.AuditScanAsync 1 20 1 3`
   - 期间执行 `HCIAbilityKit.AuditScanAsyncStop`
   - 再执行 `HCIAbilityKit.AuditScanAsyncRetry`
   - 观察 `retry start ... deep_mesh_check=true gc_every_n_batches=3`

## 5. 预期结果

- `AuditScanAsync` 第 4 参数解析成功（`gc_every_n_batches>=0`）。
- 深度模式完成日志输出扩展统计字段：
  - `batches`
  - `gc_every_n_batches`
  - `gc_runs`
  - `max_batch_ms`
  - `max_gc_ms`
  - `peak_used_physical_mib`
- 高频 GC 测试下（`gc_every_n_batches=1`）应看到 `gc_runs > 0`。
- `Retry` 不丢失 GC 节流参数。

## 6. 实际结果

- 编译：通过。
- 自动化：通过。
  - `HCIAbilityKit.Editor.AuditScanAsync`：4/4 成功（新增 `GcThrottleRetryPersistence`）。
  - `HCIAbilityKit.Editor.AuditResults`：3/3 成功（回归）。
- UE 手测：通过。
  - `HCIAbilityKit.AuditScanAsync 1 20 1 1` 正常完成至 `progress=100% processed=274/274`，无 `Error`。
  - 启动日志命中：`start total=274 batch_size=1 deep_mesh_check=true gc_every_n_batches=1`。
  - 深度统计日志命中并字段完整：
    - `batches=274`
    - `gc_every_n_batches=1`
    - `gc_runs=274`
    - `max_batch_ms=0.83`
    - `max_gc_ms=9.22`
    - `peak_used_physical_mib=2334.0`
  - 中断/重试保留 GC 配置验证通过：
    - `retry start total=274 batch_size=1 deep_mesh_check=true gc_every_n_batches=3`
    - 重试完成深度统计显示 `gc_every_n_batches=3 gc_runs=91`，参数生效。

## 7. 结论

- `Pass`

## 8. 证据

- 构建日志：`C:\Users\50428\AppData\Local\UnrealBuildTool\Log.txt`
- 自动化日志：`Saved/Logs/HCIEditorGen.log`
- 自动化关键证据：
  - `Found 4 automation tests based on 'HCIAbilityKit.Editor.AuditScanAsync'`
  - `Result={成功} Name={GcThrottleRetryPersistence}`
  - `Found 3 automation tests based on 'HCIAbilityKit.Editor.AuditResults'`
  - `Result={成功} Name={JsonSerializerIncludesCoreTraceFields}`
- UE 手测关键证据（用户回传结论）：
  - `start total=274 batch_size=1 deep_mesh_check=true gc_every_n_batches=1`
  - `[HCIAbilityKit][AuditScanAsync][Deep] ... batches=274 gc_every_n_batches=1 gc_runs=274 max_batch_ms=0.83 max_gc_ms=9.22 peak_used_physical_mib=2334.0`
  - `retry start total=274 batch_size=1 deep_mesh_check=true gc_every_n_batches=3`
  - 重试深度统计：`gc_every_n_batches=3 gc_runs=91`

## 9. 问题与后续动作

- `StageD-SliceD2` 门禁关闭，进入 `StageD-SliceD3`（峰值内存与吞吐日志监控）。
