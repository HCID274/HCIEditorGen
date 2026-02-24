# 提示词目录（Skills）

## 目标

- 将运行时提示词从代码中抽离为可审阅、可维护的文档资产。
- 对齐“文档先行”：先冻结提示词契约，再推进代码改动。
- 采用 Claude Skills 风格 Bundle：每个能力一个文件夹，固定输入输出约束。
- 当前机制选择：`Skills` 优先，`MCP` 后置到外部系统接入阶段。

## Bundle 规范

- 技能目录：`文档/提示词/Skills/<SkillName>/`
- 固定文件：
- `SKILL.md`：只写说明、描述、依赖工具（给人看）
- `prompt.md`：System Prompt 模板（支持结构化段落）
- `tools_schema.json`：严格工具 JSON Schema（约束 `args`）

## 维护规则

- 任一工具参数边界变化，必须先改对应 `tools_schema.json`，再改代码。
- `prompt.md` 允许使用占位符 `{{TOOLS_SCHEMA}}` 与 `{{USER_INPUT}}`，运行时注入。
- 不允许在 `prompt.md` 中写无关实施细节。

## 当前已收录

- `Skills/H3_AgentPlanner/`
- `Skills标准_面试谈资.md`
