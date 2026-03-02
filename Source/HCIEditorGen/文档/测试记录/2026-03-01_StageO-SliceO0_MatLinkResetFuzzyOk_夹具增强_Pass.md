# 2026-03-01_StageO-SliceO0_MatLinkResetFuzzyOk_夹具增强_Pass

## 目标

- 在不改变 `AutoMaterialSetupByNameContract` 严格契约判定的前提下，补充一个“目录分散但严格契约可通过”的 `S2_Fuzzy` 夹具，用于演示成功路径（避免总是落到 `E4403` 失败路径）。

## 变更点

- 新增 UE 控制台命令：`HCI.MatLinkResetFuzzyOk`
  - 行为：与 `HCI.MatLinkReset` 相同的“清理并重建 MatLinkChaos/Clean”流程，但 `S2_Fuzzy` 生成严格契约命名（无 `orphan_assets/unresolved_assets`）。
  - 原命令 `HCI.MatLinkReset` 保持原行为（用于失败路径与 LocateTargets）。

## 测试步骤

1. （若第一次使用）运行：`HCI.MatLinkBuildSnapshot`
2. 运行：`HCI.MatLinkResetFuzzyOk`
3. 打开：`HCI.AgentChatUI`
4. 输入（仅预览）：`扫描 /Game/__HCI_Test/Incoming/MatLinkChaos/S2_Fuzzy，然后按契约自动创建材质实例并挂贴图（仅预览）`

## 预期

- Plan 命中：`ScanAssets -> AutoMaterialSetupByNameContract`。
- DryRun 成功：`group_count>=1`。
- `orphan_assets == none` 且 `unresolved_assets == none`。
- 不产生真实写入资产（DryRun 语义保持不变）。

## 结果

- 结果：Pass（用户手测反馈）。

## 证据

- 观察口径：
  - `HCI.MatLinkResetFuzzyOk` 完成后，`/Game/__HCI_Test/Incoming/MatLinkChaos/S2_Fuzzy` 下存在 1 组严格契约命名资产（Mesh + BC/N/ORM），但目录结构为分散子目录（仍体现“fuzzy”目录形态）。
  - `AutoMaterialSetupByNameContract` 的 DryRun 不再触发 `E4403 material_setup_no_contract_groups_found`。

## 结论

- `S2_Fuzzy` 夹具现在具备两套口径：
  - `HCI.MatLinkReset`：失败夹具（非契约后缀 + orphan），用于验证失败路径与 LocateTargets。
  - `HCI.MatLinkResetFuzzyOk`：成功夹具（目录分散但严格契约可通过），用于演示“脏目录结构下仍可一把过”。

