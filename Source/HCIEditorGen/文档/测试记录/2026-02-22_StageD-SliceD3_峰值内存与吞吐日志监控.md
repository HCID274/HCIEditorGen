# StageD-SliceD3 峰值内存与吞吐日志监控（2026-02-22）

- 日期：2026-02-22
- 切片编号：StageD-SliceD3
- 测试人：助手（实现/编译/自动化）+ 用户（UE 手测）
- 当前状态：已完成（用户 UE 手测通过）

## 1. 测试目标

- 在 `StageD-SliceD2` 的基础上补齐异步扫描性能监控日志：
  - 运行中输出吞吐/ETA/内存快照（`[Perf]`）
  - 完成后输出吞吐汇总（`[PerfSummary]`，含 `avg/p50/p95`）
- 确保不破坏已有 `D1/D2` 的深度统计与重试能力。

## 2. 影响范围

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/HCIAbilityKitEditorModule.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Audit/HCIAbilityKitAuditPerfMetrics.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Tests/HCIAbilityKitAuditScanAsyncControllerTests.cpp`
- `Source/HCIEditorGen/文档/00_总进度.md`
- `Source/HCIEditorGen/文档/03_Schema_v1与错误契约.md`
- `Source/HCIEditorGen/文档/04_代码框架总览_树状图.md`
- `AGENTS.md`

## 3. 前置条件

- `StageD-SliceD2` 已通过。
- 工程编译通过。
- 存在可运行 `HCIAbilityKit.AuditScanAsync` 的 `UHCIAbilityKitAsset` 样本（建议数量较多，便于看到多次进度与 Perf 日志）。

## 4. 操作步骤

1. 编译插件：
   - `Build.bat HCIEditorGenEditor Win64 Development -Project=... -WaitMutex -FromMSBuild`
2. 自动化测试（本地已执行）：
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AuditScanAsync; Quit"`
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AuditResults; Quit"`（回归）
3. UE 手测（建议先降帧，方便观察多条日志）：
   - `t.MaxFPS = "5"`
4. 执行异步深度扫描（建议带 GC 参数，便于同时看到 `gc_runs`）：
   - `HCIAbilityKit.AuditScanAsync 1 20 1 3`
5. 观察运行中日志：
   - 仍应有标准进度日志：`[HCIAbilityKit][AuditScanAsync] progress=...`
   - 新增性能监控日志：`[HCIAbilityKit][AuditScanAsync][Perf] ...`
6. 扫描完成后观察完成态日志：
   - 仍应有摘要日志：`[HCIAbilityKit][AuditScanAsync] source=...`
   - 仍应有深度统计日志（深度模式）：`[HCIAbilityKit][AuditScanAsync][Deep] ...`
   - 新增性能汇总日志：`[HCIAbilityKit][AuditScanAsync][PerfSummary] ...`

## 5. 预期结果

- 运行中 `Perf` 日志至少包含：
  - `avg_assets_per_sec`
  - `window_assets_per_sec`
  - `eta_s`
  - `used_physical_mib`
  - `peak_used_physical_mib`
  - `gc_runs`
- 完成态 `PerfSummary` 日志至少包含：
  - `avg_assets_per_sec`
  - `p50_batch_assets_per_sec`
  - `p95_batch_assets_per_sec`
  - `max_batch_assets_per_sec`
  - `peak_used_physical_mib`
- 原有 `D1/D2` 日志不回归：
  - `[AuditScanAsync][Deep]` 统计仍输出
  - 无 Error

## 6. 实际结果

- 编译：通过（助手本地已验证）。
- 自动化：通过（助手本地已验证）。
  - `HCIAbilityKit.Editor.AuditScanAsync`：5/5 成功（新增 `PerfMetricsHelper`）。
  - `HCIAbilityKit.Editor.AuditResults`：3/3 成功（回归）。
- UE 手测：通过。
  - `HCIAbilityKit.AuditScanAsync 1 20 1 3` 完整执行至 `progress=100% processed=274/274`，全程无 `Error`。
  - 运行中多次输出 `[HCIAbilityKit][AuditScanAsync][Perf]`，并包含：
    - `avg_assets_per_sec`
    - `window_assets_per_sec`
    - `eta_s`
    - `used_physical_mib`
    - `peak_used_physical_mib`
    - `gc_runs`
  - 完成后输出 `[HCIAbilityKit][AuditScanAsync][PerfSummary]`，字段完整：
    - `assets=274 batches=274 duration_ms=2576.73 avg_assets_per_sec=106.34`
    - `p50_batch_assets_per_sec=7215.01`
    - `p95_batch_assets_per_sec=12468.82`
    - `max_batch_assets_per_sec=13698.64`
    - `peak_used_physical_mib=2142.0`
  - 原有 `[HCIAbilityKit][AuditScanAsync][Deep]` 日志仍存在，字段完整（含 `gc_every_n_batches=3 gc_runs=91 max_gc_ms=12.58 peak_used_physical_mib=2142.0`），确认 D1/D2 无回归。

## 7. 结论

- `Pass`

## 8. 证据

- 构建日志：`C:\Users\50428\AppData\Local\UnrealBuildTool\Log.txt`
- 自动化日志：`Saved/Logs/HCIEditorGen.log`
- 自动化关键证据（助手本地已验证）：
  - `Found 5 automation tests based on 'HCIAbilityKit.Editor.AuditScanAsync'`
  - `Result={成功} Name={PerfMetricsHelper}`
  - `Found 3 automation tests based on 'HCIAbilityKit.Editor.AuditResults'`
- UE 手测关键证据（用户回传结论）：
  - `[HCIAbilityKit][AuditScanAsync][Perf]` 多次出现，且包含 `avg_assets_per_sec/window_assets_per_sec/eta_s/used_physical_mib/peak_used_physical_mib/gc_runs`
  - `[HCIAbilityKit][AuditScanAsync][PerfSummary] assets=274 batches=274 duration_ms=2576.73 avg_assets_per_sec=106.34 p50_batch_assets_per_sec=7215.01 p95_batch_assets_per_sec=12468.82 max_batch_assets_per_sec=13698.64 peak_used_physical_mib=2142.0`
  - `[HCIAbilityKit][AuditScanAsync][Deep] ... gc_every_n_batches=3 gc_runs=91 max_gc_ms=12.58 peak_used_physical_mib=2142.0`

## 9. 问题与后续动作

- `StageD-SliceD3` 门禁关闭，进入 `StageE-SliceE1`（Tool Registry 能力声明冻结）。
