# StageB-SliceB0 种子清单工具链验证（2026-02-15）

- 日期：2026-02-15
- 切片编号：StageB-SliceB0
- 测试人：Codex（命令验证） + 用户（UE 手测待执行）
- 当前结论：`待你手测 Pass/Fail`

## 1. 测试目标

- 落地并验证 Stage B0 启动门禁工具链：
  - 生成 `seed_mesh_manifest`（UE 长对象路径）。
  - 校验 manifest 路径契约（拒绝 OS 路径）。
  - 支持 10k 生成脚本读取 manifest 并写入 `representing_mesh`。

## 2. 影响范围

- `SourceData/AbilityKits/Python/hci_build_seed_mesh_manifest.py`
- `SourceData/AbilityKits/Python/hci_generate_synthetic_assets.py`
- `SourceData/AbilityKits/Python/tests/test_hci_build_seed_mesh_manifest.py`
- `SourceData/AbilityKits/Python/tests/test_hci_generate_synthetic_assets.py`

## 3. 前置条件

- Python：`C:\Users\50428\AppData\Roaming\uv\python\cpython-3.11.14-windows-x86_64-none\python.exe`
- 项目根目录：`D:\1_Projects\04_GameDev\UE_Projects\HCIEditorGen`

## 4. 操作步骤（已执行）

1. 运行 Python 单测（生成脚本 + manifest 脚本）：
   - `python -m unittest -v SourceData/AbilityKits/Python/tests/test_hci_generate_synthetic_assets.py SourceData/AbilityKits/Python/tests/test_hci_build_seed_mesh_manifest.py`
2. 命令行冒烟验证：
   - `hci_build_seed_mesh_manifest.py build/validate` 子命令可执行。
   - `hci_generate_synthetic_assets.py --seed-mesh-manifest` 可执行并产出 `representing_mesh_assigned` 统计。

## 5. 预期结果

- 单测全部通过。
- manifest 校验可拒绝非 UE 长对象路径。
- 生成脚本在传入 manifest 时，为记录写入 `representing_mesh` 字段。

## 6. 实际结果（命令证据）

- 单测结果：`8/8 passed`。
- 关键日志：
  - `Generated 50 assets ... representing_mesh_assigned=50`（冒烟样例）。
- 结论：命令链路达标；UE 实机导入与手工门禁待用户执行。

## 7. 你的 UE 手测步骤（必须执行）

1. 在 UE 中导入 10~30 个高面数种子模型到 `Content/Seed/HighPoly/`（确保是 `UStaticMesh`）。
2. 在项目根目录运行：
   - `hci_build_seed_mesh_manifest.py build ... --seed-subdir Seed --seed-root /Game/Seed --name-prefix SM_`
3. 运行：
   - `hci_build_seed_mesh_manifest.py validate --manifest-file SourceData/AbilityKits/seed_mesh_manifest.json --expected-seed-root /Game/Seed`
4. 运行 10k 生成：
   - `hci_generate_synthetic_assets.py --count 10000 --seed-mesh-manifest SourceData/AbilityKits/seed_mesh_manifest.json ...`
5. 抽查 5 个 `.hciabilitykit`：
   - 必须含 `representing_mesh`，且路径形如 `/Game/.../Asset.Asset`。

## 8. Pass/Fail 标准（你反馈）

- `Pass`：
  - manifest `count > 0`；
  - validate 通过；
  - 10k 中 `representing_mesh_assigned_count == 10000`。
- `Fail`（任一命中）：
  - manifest 为空或路径不是 UE 长对象路径；
  - 生成脚本报 manifest 解析错误；
  - 产物缺失 `representing_mesh`。

## 9. 证据路径

- 单测脚本：
  - `SourceData/AbilityKits/Python/tests/test_hci_build_seed_mesh_manifest.py`
  - `SourceData/AbilityKits/Python/tests/test_hci_generate_synthetic_assets.py`
- 生成脚本：
  - `SourceData/AbilityKits/Python/hci_build_seed_mesh_manifest.py`
  - `SourceData/AbilityKits/Python/hci_generate_synthetic_assets.py`
