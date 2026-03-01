# Stage N-SliceN4 测试记录（Pass）

日期：2026-02-27

## 目标

- 脏路径输入（粘连中文、缺斜杠、路径中夹空格、`_HCI_Test`/`__HCI_Test` 混用）不再导致空扫描或路由失败。
- Planner 在 `ENV_CONTEXT` 中获得“已通过 AssetRegistry 校验”的候选路径，并优先使用合法路径。
- 当 DryRun 预览发现 `ScanAssets.asset_count==0` 且疑似路径非法时，自动压栈并重试规划 1 次。
- `NormalizeAssetNamingByMetadata` 整理过程中有右下角转圈通知（DryRun/Execute）。

## 前置条件

- 仅使用测试目录：`/Game/__HCI_Test/...`
- `Seed` 目录为只读数据源，不允许写入：`/Game/Seed`

## 步骤

1. UE 控制台执行：`HCI.SeedChaosReset`
2. 打开：`HCI.AgentChatUI`
3. 输入（故意脏路径）：
   - `扫描Game/__HCI_Test/Incoming/SeedChaos，然后按元数据规范命名并移动归档到，Game/__HCI_Test/   Organized/SeedClean.`
4. 观察：
   - 日志包含 `[HCIAbilityKit][AgentChatUI][NormalizeInput] before=... after=...`
   - 计划命中 `ScanAssets -> NormalizeAssetNamingByMetadata`
   - 空扫描/路径不合法时触发一次自动重试（压栈纠错）
   - `NormalizeAssetNamingByMetadata` DryRun/Execute 弹出右下角转圈通知并更新阶段文本

## 结果

- Pass（UE 手测已确认）。
