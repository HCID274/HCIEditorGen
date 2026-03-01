# 2026-02-25 StageI-SliceI8 ChatUI 一屏闭环与自动弹窗修复

## 1. 目标

- 在 `AgentChatUI` 内完成“输入 -> 计划可视化 -> 预览/执行 -> 结果回写”闭环。
- 修复交互缺陷：聊天链路下计划生成后不应自动弹出 `PlanPreview`，必须由用户点击卡片按钮主动打开。

## 2. 变更范围（冻结）

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Public/Commands/HCIAbilityKitAgentCommandBase.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Commands/HCIAbilityKitAgentCommand_ChatPlanAndSummary.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Public/Subsystems/HCIAbilityKitAgentSubsystem.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Subsystems/HCIAbilityKitAgentSubsystem.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/UI/HCIAbilityKitAgentChatWindow.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/UI/HCIAbilityKitAgentPlanPreviewWindow.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/UI/HCIAbilityKitAgentPlanPreviewWindow.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Commands/HCIAbilityKitAgentCommandHandlers.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Commands/HCIAbilityKitAgentDemoConsoleCommands.cpp`

## 3. 实施步骤

1. 增加 `CommandResult` 结构化计划载荷（Plan/RouteReason/PlannerMetadata）。
2. Subsystem 缓存最近计划并新增 `OnPlanReady` 事件。
3. ChatUI 新增“最新计划（I8）”卡片与 `打开预览/确认并执行` 按钮。
4. 抽取 `PlanPreviewWindow::ExecutePlan` 供 Chat 与 Preview 共用执行口径。
5. 为 `HCI_RequestAgentPlanPreviewFromUi` 增加 `bAutoOpenPreviewWindow` 开关，Chat 场景传 `false`。

## 4. UE 手测门禁

1. `HCI.AgentChatUI`
2. 输入：`帮我看看那个MNew文件夹里有什么`
3. 预期：出现计划卡片；不自动弹窗；点击 `打开预览` 后才弹出 `PlanPreview`。
4. 输入：`整理临时目录资产并归档`
5. 预期：计划卡片更新；点击 `确认并执行` 后显示确认框并在聊天区回写执行结果。

## 5. 结果

- 本地实现：完成
  - 计划卡片、主动预览、聊天内执行回写均已落地。
  - 自动弹窗缺陷已修复（`auto_open=false`）。
- 本地验证：通过
  - 编译：`Build.bat HCIEditorGenEditor Win64 Development ... -NoHotReloadFromIDE` 通过（exit code 0）。
- UE 手测：通过
  - 用户门禁反馈：`Pass`
- 结论：Pass（I8 关闭）。

