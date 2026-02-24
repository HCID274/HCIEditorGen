# Skill: H3_AgentPlanner

## 目标

- 将自然语言请求规划为 `Plan JSON`。
- 严格约束 `steps[].args`，避免字段漂移（例如 `directory` 之类未声明字段）。

## 适用范围

- Stage H 的 LLM 规划入口（Real HTTP Provider）。
- 仅负责 `Plan` 生成，不负责执行与写入。

## 依赖

- Tool Registry 冻结白名单（含 SearchPath）与参数边界。
- `tools_schema.json` 中声明的 `args` JSON Schema。

## 文件职责

- `SKILL.md`：技能说明与维护规则（本文件）。
- `prompt.md`：System Prompt 模板，包含 `{{TOOLS_SCHEMA}}`、`{{ENV_CONTEXT}}` 与 `{{USER_INPUT}}` 占位符。
- `tools_schema.json`：严格工具约束，是 `args` 边界唯一文档源。

## 维护规则

- 工具参数有任何增删改，先改 `tools_schema.json`，再改代码实现。
- `prompt.md` 只保留可执行提示词内容，不写过程说明。
- 输出契约变更必须先同步到主进度文档与测试门禁文档。
