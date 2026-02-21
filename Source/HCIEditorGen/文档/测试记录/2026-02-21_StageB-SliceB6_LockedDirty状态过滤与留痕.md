# StageB-SliceB6 Locked/Dirty 状态过滤与留痕（2026-02-21）

- 日期：2026-02-21
- 切片编号：StageB-SliceB6
- 测试人：Codex（实现/编译）+ 用户（UE 手测）
- 当前结论：`Pass`

## 1. 测试目标

- 在 B5 基础上补齐 B6 门禁能力：
  - 扫描阶段识别并跳过 `Locked/Dirty` 风险资产；
  - 行日志输出 `scan_state/skip_reason`；
  - 摘要输出 `skipped_locked_or_dirty` 计数。

## 2. 影响范围

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Audit/HCIAbilityKitAuditScanService.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Audit/HCIAbilityKitAuditScanService.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/HCIAbilityKitEditorModule.cpp`

## 3. 前置条件

- 项目：`D:\1_Projects\04_GameDev\UE_Projects\HCIEditorGen`
- UE：`5.4.4`
- B5 已 `Pass`
- 编译通过：`Build.bat HCIEditorGenEditor Win64 Development ...`（exit code 0）

## 4. 操作步骤（已执行）

1. 基线扫描：
   - `HCIAbilityKit.AuditScan 20`
2. 制造 Dirty 资产（修改 AbilityKit 资产字段且不保存）。
3. 再次扫描并观察留痕：
   - `HCIAbilityKit.AuditScan 20`
4. 异步链路一致性：
   - `HCIAbilityKit.AuditScanAsync 1 20`
   - `HCIAbilityKit.AuditScanProgress`
5. （可选）只读路径：
   - 将目标 `.uasset` 标记只读后再次扫描，观察 `package_read_only`。

## 5. 预期结果

- 摘要包含 `skipped_locked_or_dirty=...`。
- 行日志包含：
  - `scan_state=ok`（正常资产）
  - 或 `scan_state=skipped_locked_or_dirty`
  - `skip_reason=package_dirty/package_read_only`
- 同步与异步两条扫描链路字段口径一致。

## 6. 实际结果

- 用户完成手测并反馈 `Pass`。
- 字段口径与门禁目标一致，进入下一切片。

## 7. 结论

- `Pass`

## 8. 证据

- 用户门禁信号：`pass`（当前会话）。
- 代码侧已落地：
  - 摘要字段：`skipped_locked_or_dirty`
  - 行字段：`scan_state/skip_reason`
  - 跳过原因：`package_dirty/package_read_only`

## 9. 后续动作

- 进入 `StageB-SliceB7`：数据驱动预览体（Actor 读取 `RepresentingMesh`，支持拖入场景可视化验证）。
