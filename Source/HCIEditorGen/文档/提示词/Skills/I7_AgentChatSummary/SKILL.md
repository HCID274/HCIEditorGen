# Skill: I7_AgentChatSummary

## 目标

- 将已生成的 Plan 上下文压缩为聊天窗口可读摘要。
- 摘要生成必须走 LLM + Prompt，不允许本地模板拼接替代。

## 适用范围

- Stage I-SliceI7 的 `AgentChatUI` 结果回写。
- 仅负责“摘要表达”，不参与规划路由与执行决策。

## 文件职责

- `SKILL.md`：技能说明与维护规则（本文件）。
- `prompt.md`：摘要提示词模板，包含 `{{TOOLS_SCHEMA}}`、`{{ENV_CONTEXT}}`、`{{USER_INPUT}}`。
- `tools_schema.json`：占位约束文档（本技能不调用工具，但保持 Bundle 契约一致）。
- `quick_commands.json`：聊天窗口快捷指令配置（标签与自然语言模板）。

## 维护规则

- 快捷指令文案修改必须优先改 `quick_commands.json`，禁止在 C++ 写死自然语言模板。
- 摘要字段变更必须先改 `prompt.md` 输出契约，再改 UI 渲染逻辑。
