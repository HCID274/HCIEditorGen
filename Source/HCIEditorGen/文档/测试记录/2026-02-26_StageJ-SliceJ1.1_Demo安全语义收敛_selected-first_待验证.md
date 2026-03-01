# Stage J-SliceJ1.1 测试记录：Demo 安全语义收敛（selected-first）

- 测试日期：2026-02-26
- 测试人：助手（本地自动化）+ 用户（UE 手测）
- 状态：**已通过（Pass）**

## 1. 测试目标
验证 `ScanLevelMeshRisks` 在 `scope=selected` 且未选中任何 Actor 时，是否按照安全门禁返回失败并给出 `ErrorCode=no_actors_selected`；同时验证工具契约已包含可选参数 `actor_names`。

## 2. 本地自动化验证（已完成）

### 2.1 编译验证
- 命令：
  - `Build.bat HCIEditorGenEditor Win64 Development D:/1_Projects/04_GameDev/UE_Projects/HCIEditorGen/HCIEditorGen.uproject -NoHotReloadFromIDE`
- 结果：通过（ExitCode=0）。

### 2.2 自动化测试验证
- 用例 1：
  - `HCI.Editor.AgentTools.ArgsSchemaFrozen`
  - 结果：通过。
  - 证据：`Saved/Logs/HCIEditorGen.log` 中存在 `Test Completed. Result={成功} Name={ArgsSchemaFrozen}`。
- 用例 2：
  - `HCI.Editor.AgentTools.ScanLevelMeshRisksSelectedWithoutSelectionFails`
  - 结果：通过。
  - 证据：`Saved/Logs/HCIEditorGen.log` 中存在 `Test Completed. Result={成功} Name={ScanLevelMeshRisksSelectedWithoutSelectionFails}`。

## 3. UE 手测执行记录（已完成）

### 步骤 1：未选中对象触发扫描（通过）
- 操作：
  1. 在关卡中取消所有 Actor 选择。
  2. 打开 `HCI.AgentChatUI`，输入“扫描选中的物体风险”或同义指令。
  3. 执行到 `ScanLevelMeshRisks(scope=selected)`。
- 结果：
  1. 规划命中 `scope=selected`。
  2. 执行终态为 `completed_with_failures`（`succeeded=0 failed=1`），符合空选择失败预期。

### 步骤 2：选中单个对象再扫描（通过）
- 操作：
  1. 选中 1 个有静态网格的 Actor。
  2. 重复同样指令并执行扫描。
- 结果：
  1. 扫描成功，执行终态 `completed`（`succeeded=1 failed=0`）。
  2. 日志显示 `scanned_level_actors=1`、`risky_level_actors=1`，且计划仍为 `scope=selected`。

## 4. 结论
- 本地自动化：**通过**。
- UE 手测门禁：**通过（用户反馈 `Pass`）**。
- 结论：`Stage J-SliceJ1.1` 收口完成，可进入下一切片选择。
