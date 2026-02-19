# StageB-SliceB0 Seed 命名与面数检查（2026-02-19）

- 日期：2026-02-19
- 切片编号：StageB-SliceB0
- 测试人：Codex（UE 命令验证）

## 1. 测试目标

- 在不进入后续数据生成流程的前提下，先完成 Seed 资产体检：
  - 检查 `/Game/Seed` 下资产类型分布；
  - 检查 `StaticMesh` 命名合规（`^SM_[A-Za-z0-9_]+$`）；
  - 读取可用面数（LOD0 Triangles 标签）。
- 在用户授权后，执行一次批量命名整改并复检。

## 2. 影响范围

- `SourceData/AbilityKits/Python/hci_inspect_seed_assets.py`
- `SourceData/AbilityKits/Python/hci_rename_seed_meshes.py`
- `SourceData/AbilityKits/seed_mesh_inspection_report.json`
- `SourceData/AbilityKits/seed_mesh_rename_report.json`

## 3. 操作步骤

1. 运行 UE 命令行检查脚本（只读）：
   - `hci_inspect_seed_assets.py`
2. 读取首轮检查报告。
3. 运行 UE 命令行重命名脚本（先 dry-run，再实执行）：
   - `hci_rename_seed_meshes.py --dry-run`
   - `hci_rename_seed_meshes.py`
4. 再次运行检查脚本复检：
   - `hci_inspect_seed_assets.py`

## 4. 预期结果

- 首轮检查可定位命名不合规项与高面数项。
- 批量重命名后，`StaticMesh` 命名不合规数归零。
- 面数统计不丢失，且不触发脚本失败。

## 5. 实际结果

- 首轮检查：
  - `total_assets_under_seed=191`
  - `static_mesh_count=12`
  - `non_static_asset_count=179`
  - `naming_noncompliant_count=12`
  - `triangle_known_count=12`
  - `high_poly_ge_50000_count=9`
- 重命名执行：
  - `renamed_count=12`
  - `failed_count=0`
- 复检结果：
  - `naming_noncompliant_count=0`
  - `static_mesh_count=12`（保持不变）
  - `high_poly_ge_50000_count=9`（保持不变）

## 6. 结论

- `Pass`（本次“命名整改与面数体检”子任务）

## 7. 证据路径

- 检查报告：`SourceData/AbilityKits/seed_mesh_inspection_report.json`
- 重命名报告：`SourceData/AbilityKits/seed_mesh_rename_report.json`
- 脚本：
  - `SourceData/AbilityKits/Python/hci_inspect_seed_assets.py`
  - `SourceData/AbilityKits/Python/hci_rename_seed_meshes.py`
