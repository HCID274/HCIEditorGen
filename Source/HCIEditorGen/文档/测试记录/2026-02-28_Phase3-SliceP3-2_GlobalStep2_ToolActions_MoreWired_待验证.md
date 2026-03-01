# 2026-02-28 Phase3-SliceP3-2_GlobalStep2_ToolActions_MoreWired（待验证）

## 目标

- 全局 Step 2：继续拆分已接线 ToolActions（保持语义不变）。
- 修复 AgentChatUI：同一条“消息型回复”在 UI 中重复出现两次的问题。

## 变更范围（摘要）

- ToolActions 拆分：`SearchPath / AutoMaterialSetupByNameContract / RenameAsset / MoveAsset` 拆出到独立编译单元，并在 `BuildStageIDraftActions` 走 factories 接线。
- ChatUI 重复消息修复：消息型结果不再同时走 `OnChatLine + OnSummaryReceived` 两条 UI 渲染链路。

## 复现步骤（Fail 记录）

1. 打开：`HCI.AgentChatUI`
2. 输入：`搜索 MNew 目录`
3. 观察：同一条系统回复在聊天窗口中出现两次（重复气泡）。

## 预期

- 同一条系统回复只出现一次（由 “思考中...” 气泡收敛成单条结果）。

## 修复点（实现口径）

- 文件：`Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Subsystems/HCIAbilityKitAgentSubsystem.cpp`
- 位置：`HandleCommandCompleted` 的“非可执行计划（message-only）成功分支”
- 调整：仅触发 `OnSummaryReceived` 驱动 thinking bubble 收敛，不再 `EmitAssistantLine` 同步发一条 chat line，避免重复展示。

## 复测门禁（待用户 UE 手测）

- 复测同上步骤 1~3，确认不再重复后标记：`Pass`。

## 结果

- 结论：`Pass`（用户 UE 手测反馈）
- 时间：2026-02-28
