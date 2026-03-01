# 测试记录：Stage N-SliceN2（Asset Routing 多级目录归档）

日期：2026-02-27  
版本：未发布（本地开发态）  
目标：验证 `NormalizeAssetNamingByMetadata` 在 `target_root` 下执行“资产组文件夹 + 子目录（Textures/Materials/）”分发，并保持 HITL 审批与 Redirector FixUp 安全性。

## 1. 前置条件

- UE 工程：`HCIEditorGen`
- 已存在 Stage N-SliceN1：外部投递清单 + UE 导入命令可用
- 可选规则文件（用于验证可配置路由）：
  - `Saved/HCIAbilityKit/Rules/Project_Rules.md`
  - 包含 `hci_routing_json` 代码块（见 Stage N 文档示例）

## 2. 测试数据

- 外部投递目录：任意包含 `png/jpg`（可选 `fbx`）的文件夹
- 建议文件名（便于观测路由效果）：
  - `barrel_basecolor.png`
  - `barrel_normal.png`

## 3. 步骤

1. 外部投递：
   - `python SourceData/AbilityKits/Python/hci_ingest_stage.py --input <测试目录> --source-app manual --suggest-target-root /Game/__HCI_Test/Incoming`
2. UE 导入：
   - `HCI.IngestDumpLatest`
   - `HCI.IngestImportLatest`（确认导入）
3. Agent 整理：
   - `HCI.AgentChatUI`
   - 输入：`帮我规范化刚导入的批次，并归档到 /Game/__HCI_Test/Organized`
4. 审批：
   - 在写审批卡点击 `通过`

## 4. 预期

- 计划命中 `ScanAssets -> NormalizeAssetNamingByMetadata`，且写步骤 `requires_confirm=true`。
- 执行后目标目录结构出现：
  - `.../<AssetGroup>/Textures/` 至少包含一个 `T_` 资产。
- 原始导入路径不再存在或仅保留 Redirectors（且 Redirectors 被 FixUp）。
- 执行过程无崩溃；日志可定位（失败需包含路径/原因）。

## 5. 结果

- 结果：Pass（用户 UE 手测通过）
- 证据（关键日志）：
  - 规划命中 `ScanAssets -> NormalizeAssetNamingByMetadata`，写步骤 `requires_confirm=true`。
  - 执行完成：
    - `terminal=completed terminal_reason=executor_execute_completed succeeded=2 failed=0`
  - 路由落点（真实路径）：
    - `/Game/__HCI_Test/Organized/Barrel/Textures/T_Barrel_BC.T_Barrel_BC`
    - `/Game/__HCI_Test/Organized/Barrel/Textures/T_Barrel_N.T_Barrel_N`
  - 资产验证：
    - `LogContentValidation: Validating asset /Game/__HCI_Test/Organized/Barrel/Textures/T_Barrel_BC...`
    - `LogContentValidation: Validating asset /Game/__HCI_Test/Organized/Barrel/Textures/T_Barrel_N...`

## 6. 结论

- `Pass/Fail`：Pass
