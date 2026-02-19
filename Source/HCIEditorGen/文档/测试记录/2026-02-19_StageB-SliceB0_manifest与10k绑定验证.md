# StageB-SliceB0 Manifest 与 10k 引用链绑定验证（2026-02-19）

- 日期：2026-02-19
- 切片编号：StageB-SliceB0
- 测试人：Codex（命令验证）+ 用户（UE 手测待执行）
- 当前结论：`待你手测 Pass/Fail`

## 1. 测试目标

- 固化 Stage B0 启动门禁的命令链结果：
  - 生成 `seed_mesh_manifest`（仅 UE 长对象路径，且前缀为 `SM_`）；
  - 校验 manifest 契约；
  - 运行 10k 资产生成并验证 `representing_mesh` 全量绑定。

## 2. 影响范围

- `SourceData/AbilityKits/Python/hci_build_seed_mesh_manifest.py`
- `SourceData/AbilityKits/Python/hci_generate_synthetic_assets.py`
- `SourceData/AbilityKits/seed_mesh_manifest.json`
- `SourceData/AbilityKits/Generated_B0_20260219/`

## 3. 前置条件

- Python：
  - `C:\Users\50428\AppData\Roaming\uv\python\cpython-3.11.14-windows-x86_64-none\python.exe`
- 项目根目录：
  - `D:\1_Projects\04_GameDev\UE_Projects\HCIEditorGen`
- Seed 状态：
  - `/Game/Seed` 下 StaticMesh 已命名规范化为 `SM_*`（见 2026-02-19 命名体检记录）。

## 4. 操作步骤（已执行）

1. 生成 manifest：
   - `hci_build_seed_mesh_manifest.py build --project-root . --content-dir Content --seed-subdir Seed --seed-root /Game/Seed --output-file SourceData/AbilityKits/seed_mesh_manifest.json --name-prefix SM_`
2. 校验 manifest：
   - `hci_build_seed_mesh_manifest.py validate --manifest-file SourceData/AbilityKits/seed_mesh_manifest.json --expected-seed-root /Game/Seed`
3. 生成 10k 数据并绑定 seed mesh：
   - `hci_generate_synthetic_assets.py --count 10000 --seed-mesh-manifest SourceData/AbilityKits/seed_mesh_manifest.json --output-dir SourceData/AbilityKits/Generated_B0_20260219`

## 5. 预期结果

- manifest 生成成功且 `count > 0`。
- validate 通过，路径均为 UE 长对象路径且位于 `/Game/Seed`。
- 10k 生成完成，`representing_mesh_assigned=10000`。

## 6. 实际结果（命令证据）

- manifest 生成：
  - `[seed-manifest] wrote 12 entries to .../SourceData/AbilityKits/seed_mesh_manifest.json`
- manifest 校验：
  - `[seed-manifest] validation passed: .../SourceData/AbilityKits/seed_mesh_manifest.json`
- 10k 生成：
  - `Generated 10000 assets into SourceData\AbilityKits\Generated_B0_20260219 (semantic_gap=3000, trap=1200, performance_trap=600, representing_mesh_assigned=10000).`
- 完整性抽检：
  - `TOTAL=10000 MISSING_REPRESENTING_MESH=0`

## 7. 结论

- 命令链结论：`Pass`
- 切片门禁结论：`待你在 UE 手测后给出 StageB-SliceB0 总体 Pass/Fail`

## 8. 你的 UE 手测步骤（必须执行）

1. 在 UE 中抽查 `SourceData/AbilityKits/seed_mesh_manifest.json` 对应的 12 个 `/Game/Seed/SM_*` 资产可正常定位。
2. 在 `SourceData/AbilityKits/Generated_B0_20260219/` 抽查 5 个 `.hciabilitykit`：
   - 必须包含 `representing_mesh`；
   - 值必须形如 `/Game/.../Asset.Asset`。
3. 执行你既定导入流程，确认无路径解析错误（特别是 `E1006`）。
4. 在群聊回复：`StageB-SliceB0 Pass` 或 `StageB-SliceB0 Fail + 错误现象`。

## 9. 证据路径

- `SourceData/AbilityKits/seed_mesh_manifest.json`
- `SourceData/AbilityKits/Generated_B0_20260219/generation_report.json`
- `SourceData/AbilityKits/Python/hci_build_seed_mesh_manifest.py`
- `SourceData/AbilityKits/Python/hci_generate_synthetic_assets.py`
