# Stage J-SliceJ3 测试记录：命名与归档自动化（NormalizeAssetNamingByMetadata）

- 测试日期：2026-02-26
- 测试人：助手（本地自动化）+ 用户（UE 手测）
- 状态：**已通过**

## 1. 测试目标
验证 `NormalizeAssetNamingByMetadata` 已接入真实执行链路。通过 Agent 指令触发后，工具应给出可审阅的命名/归档提案，并在确认执行后真实修改资产路径与命名（而非 simulated）。

## 2. 测试环境
- 目录：`/Game/__HCI_Auto/J3_*`（隔离测试目录）。
- 资产：至少包含 `Texture2D` 与 `StaticMesh` 各 1 个。
- 启动方式：本地 `HCIEditorGenEditor`。

## 3. 测试步骤与预期

**步骤 1：DryRun/Execute 提案闭环**
- 操作：打开 UE，执行控制台命令 `HCI.AgentChatUI`。
- 操作：输入 `把 /Game/__HCI_Auto 下资产按元数据规范命名并归档到 /Game/__HCI_Auto/Organized，并执行修改`。
- 预期：
  1. 计划命中 `ScanAssets -> NormalizeAssetNamingByMetadata`；
  2. `NormalizeAssetNamingByMetadata` 步骤为写操作（`requires_confirm=true`）；
  3. 执行后输出 `normalize_asset_naming_by_metadata_execute_ok`（或结构化失败回执）。

**步骤 2：资产落盘验收**
- 操作：在 Content Browser 检查 `target_root=/Game/__HCI_Auto/Organized`。
- 预期：
  1. 至少 1 个资产被迁移到 `Organized` 子目录；
  2. 资产名命中前缀规范（如 `SM_` / `T_`）；
  3. 原路径引用未脏写崩溃（允许出现 redirector 修复日志）。

## 4. 结论与证据
- **本地自动化结论**：通过。
- **本地自动化证据（2026-02-26）**：
  1. 编译通过：`Build.bat HCIEditorGenEditor Win64 Development ... -NoHotReloadFromIDE`（ExitCode=0）。
  2. 用例通过：`HCI.Editor.AgentTools.NormalizeAssetNamingByMetadataDryRunBuildsProposals`。
  3. 用例通过：`HCI.Editor.AgentTools.NormalizeAssetNamingByMetadataExecuteRenamesAndMovesAssets`。
  4. 回归通过：`Automation RunTests HCI.Editor.AgentTools`（13/13，`**** TEST COMPLETE. EXIT CODE: 0 ****`）。
- **UE 手测门禁结论**：通过（用户反馈 `Pass`）。
- **UE 手测证据（2026-02-26）**：
  1. 计划命中：`ScanAssets -> NormalizeAssetNamingByMetadata`，写步骤 `requires_confirm=true`。
  2. 执行命中：`terminal=completed`、`execution_mode=execute_apply`、`succeeded=2 failed=0`。
  3. 落盘命中：3 个资产保存到 `/Game/__HCI_Auto/Organized/*`（`T_...` / `SM_...` 前缀规范）。
  4. 校验命中：`LogContentValidation` 对 3 个已归档资产完成验证。
