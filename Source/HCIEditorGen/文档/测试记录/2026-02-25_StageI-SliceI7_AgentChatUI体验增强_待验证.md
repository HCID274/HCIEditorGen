# 2026-02-25 StageI-SliceI7 AgentChatUI 体验增强

## 1. 目标

- 在不改变主链路语义的前提下增强聊天入口可用性。
- 完成三项能力：`历史持久化`、`快捷指令外置配置`、`Prompt 驱动摘要回写`。

## 2. 变更范围（冻结）

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/UI/HCIAbilityKitAgentChatWindow.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Commands/HCIAbilityKitAgentCommands_Core.cpp`
- `Source/HCIEditorGen/文档/00_总进度.md`
- `Source/HCIEditorGen/文档/05_开发执行总方案_资产审计.md`
- `AGENTS.md`

## 3. 实施步骤

1. 先冻结 I7 文档目标、边界与 UE 门禁。
2. 实现 ChatWindow 三项增强（持久化、快捷指令外置、Prompt 摘要回写）。
3. 本地编译验证通过。
4. 提供 UE 手测门禁步骤并等待用户 Pass/Fail。

## 4. UE 手测门禁

1. 控制台输入：`HCIAbilityKit.AgentChatUI`
2. 在聊天窗口点击 `查看MNew目录`，确认自动发送并命中真实规划：
   - 日志出现 `[HCIAbilityKit][AgentPlanLLM][H3] dispatched ... source=AgentChatUI`
   - 自动弹出 `PlanPreview`
3. 关闭聊天窗口后再次执行 `HCIAbilityKit.AgentChatUI`：
   - 预期历史区自动恢复上次消息（持久化生效）
4. 再次发送 `整理临时目录资产并归档`（可用快捷按钮）：
   - 预期聊天区出现由 LLM 生成的摘要文案（来自 `Skills/I7_AgentChatSummary/prompt.md`）
   - 非验收口径：本地拼接 `intent/route_reason/steps/provider/fallback` 模板摘要

## 5. 结果

- 文档冻结：完成
- 代码实现：完成
- 本地验证：通过
  - 编译：
    - `Build.bat HCIEditorGenEditor Win64 Development ... -NoHotReloadFromIDE` 通过（exit code 0）
- UE 手测：通过
  - 证据 1（快捷指令来源）：
    - `[HCIAbilityKit][AgentPlanLLM][H3] dispatched ... source=AgentChatUI` 多次命中。
  - 证据 2（快捷指令外置）：
    - `查看MNew目录 / 整理临时目录资产并归档 / 扫描关卡里缺碰撞和默认材质` 三条指令均能正常路由并生成 Plan。
  - 证据 3（Prompt 驱动摘要）：
    - `[HCIAbilityKit][AgentChatUI][Summary] dispatched model=... bundle=Source/HCIEditorGen/文档/提示词/Skills/I7_AgentChatSummary`
    - `[HCIAbilityKit][AgentChatUI][Summary] done success=true status=200`
- 风险留痕：
  - 在“整理临时目录资产并归档”链路观测到既有 `fallback_reason=llm_contract_invalid (E4303)` 与 `/Game/Temp` 资产缺失告警。
  - 该问题归属 H3 契约稳定性历史问题，不属于 I7 新回归。
- 结论：Pass（I7 关闭）
