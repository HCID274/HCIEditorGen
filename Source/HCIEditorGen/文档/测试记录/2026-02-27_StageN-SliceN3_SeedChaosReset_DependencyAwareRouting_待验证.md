# 测试记录：Stage N-SliceN3（SeedChaos 可重复回归 + 依赖感知目录重整）

日期：2026-02-27  
版本：未发布（本地开发态）  
目标：验证在“真实混乱目录（目录打散 + 名字变脏）”场景下，`NormalizeAssetNamingByMetadata` 能基于 `AssetRegistry` 依赖关系将 `Mesh -> Materials -> Textures` 归并，并对交叉引用资源落点到 `Shared/`；同时验证夹具命令可无限次 Reset 回归测试。

## 1. 前置条件

- UE 工程：`HCIEditorGen`
- `Stage N-SliceN2` 已通过：审批卡能展示具体目录落点（DryRun Preview）
- 本片新增夹具命令（Editor 控制台）：
  - `HCI.SeedChaosBuildSnapshot`
  - `HCI.SeedChaosReset`

## 2. 测试数据

- Seed 原始目录（只读）：`/Game/Seed`（对应磁盘：`Content/Seed`）
- Snapshot（生成态）：`/Game/__HCI_Test/Fixtures/SeedChaosSnapshot`
- Incoming（混乱态）：`/Game/__HCI_Test/Incoming/SeedChaos`
- Organized（整理态）：`/Game/__HCI_Test/Organized/SeedClean`

## 3. 步骤

1. 生成快照（首次运行）：
   - `HCI.SeedChaosBuildSnapshot`
2. 每次回归重置：
   - `HCI.SeedChaosReset`
   - 预期会弹确认框，仅删除并重建：
     - `/Game/__HCI_Test/Incoming/SeedChaos`
     - `/Game/__HCI_Test/Organized/SeedClean`
3. 运行 Agent 整理：
   - `HCI.AgentChatUI`
   - 输入：`扫描 /Game/__HCI_Test/Incoming/SeedChaos，然后按元数据规范命名并移动归档到 /Game/__HCI_Test/Organized/SeedClean。`
4. 审批：
   - 写审批卡点击 `通过`

## 4. 预期

- `SeedChaosReset` 后：
  - Incoming 内资产被打散到多个子目录，且 asset name 明显变脏（如 `SM_xxxxxxxx_final_v2`）。
  - `Saved/HCIAbilityKit/TestFixtures/SeedChaos/latest.json` 存在并记录 dirty moves/renames。
- 计划命中 `ScanAssets -> NormalizeAssetNamingByMetadata`，写步骤 `requires_confirm=true`。
- 审批卡能显示：
  - “将分发到目录：”包含多个 `<Group>/Textures`，并在存在共享资源时包含 `.../Shared/...`
  - “目录迁移清单（示例）”显示 `source -> destination` 完整路径。
- 执行后：
  - 资产分散归档到多个资产组目录（不再扁平堆叠）。
  - Shared 资源进入 `.../Shared/...`（如果存在交叉引用）。
  - 不出现引用丢失（Redirectors 被 FixUp，无明显丢材质/丢贴图现象）。

## 5. 结果

- 结果：待 UE 手测
- 证据（粘贴关键日志/截图）：待补充

## 6. 结论

- `Pass/Fail`：待定

