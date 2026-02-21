# StageB-SliceB5 triangle_count Tag 采集与来源标记（2026-02-21）

- 日期：2026-02-21
- 切片编号：StageB-SliceB5
- 测试人：Codex（实现/编译）+ 用户（UE 手测）
- 当前结论：`Pass`

## 1. 测试目标

- 在 B4 基础上补齐 B5 门禁能力：
  - 采集 `triangle_count` 相关 Tags（若存在）；
  - 记录并输出 `triangle_source=tag_cached`；
  - 在摘要中输出 `triangle_tag_coverage`。

## 2. 影响范围

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Audit/HCIAbilityKitAuditTagNames.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Audit/HCIAbilityKitAuditScanService.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Audit/HCIAbilityKitAuditScanService.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/HCIAbilityKitEditorModule.cpp`

## 3. 前置条件

- 项目：`D:\1_Projects\04_GameDev\UE_Projects\HCIEditorGen`
- UE：`5.4.4`
- B4 已 `Pass`
- 项目内存在带三角相关 Tag 的种子网格（日志中命中 `Triangles`）

## 4. 操作步骤（已执行）

1. 编译验证：
   - `Build.bat HCIEditorGenEditor Win64 Development -Project="D:/1_Projects/04_GameDev/UE_Projects/HCIEditorGen/HCIEditorGen.uproject" -WaitMutex -FromMSBuild`
2. UE 手测（用户）：
   - `HCIAbilityKit.AuditScan 20`
   - `HCIAbilityKit.AuditScanAsync 1 20`
   - `HCIAbilityKit.AuditScanProgress`

## 5. 预期结果

- 摘要日志包含 `triangle_tag_coverage=...`；
- `row=` 日志包含：
  - `triangle_count_lod0=...`
  - `triangle_source=...`
  - `triangle_source_tag=...`
- 若种子网格包含三角 Tag，至少一条命中 `triangle_source=tag_cached`。

## 6. 实际结果

- 用户手测 `AuditScan` 命中：`triangle_tag_coverage=99.5%`。
- 用户手测 `AuditScanAsync` 命中：`triangle_tag_coverage=99.5%`。
- `row=` 日志完整输出新字段：
  - 无数据样本：`triangle_count_lod0=-1 triangle_source=unavailable triangle_source_tag=`
  - 有数据样本：`triangle_count_lod0=196608 triangle_source=tag_cached triangle_source_tag=Triangles`
- 多条记录命中 `triangle_source=tag_cached`，来源键为 `Triangles`。
- `AuditScanProgress` 返回 `progress=idle`，符合扫描完成态。

## 7. 结论

- `Pass`
- 判定依据：
  - B5 目标字段已在同步/异步扫描两条链路生效；
  - `triangle_source=tag_cached` 路径在实机日志可复现；
  - 摘要统计口径与行级证据一致。

## 8. 证据

- 用户 UE 日志关键片段（摘要）：
  - `[HCIAbilityKit][AuditScan] ... triangle_tag_coverage=99.5% ...`
  - `[HCIAbilityKit][AuditScanAsync] ... triangle_tag_coverage=99.5% ...`
- 用户 UE 日志关键片段（行级）：
  - `triangle_count_lod0=-1 triangle_source=unavailable triangle_source_tag=`
  - `triangle_count_lod0=196608 triangle_source=tag_cached triangle_source_tag=Triangles`
- 用户 UE 日志关键片段（进度）：
  - `[HCIAbilityKit][AuditScanAsync] progress=idle`

## 9. 后续动作

- 进入 `StageB-SliceB6`：实现 `Locked/Dirty` 状态过滤与跳过留痕。
