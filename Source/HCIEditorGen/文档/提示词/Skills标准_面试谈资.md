# Skills Bundle 标准（面试谈资）

## 一、标准定义（已冻结）

- 提示词目录采用 Claude Skills 风格 Bundle：
- `文档/提示词/Skills/<SkillName>/SKILL.md`
- `文档/提示词/Skills/<SkillName>/prompt.md`
- `文档/提示词/Skills/<SkillName>/tools_schema.json`

## 二、三文件职责

- `SKILL.md`：能力说明、适用范围、依赖工具、维护规则（给人看）。
- `prompt.md`：System Prompt 模板（SOP 思考流），仅放可执行约束。
- `tools_schema.json`：严格参数契约（`additionalProperties=false` + 枚举/范围/正则）。

## 三、运行时注入规则

- Prompt 构建必须来自 Bundle，不允许硬编码 System Prompt。
- `prompt.md` 必须包含占位符：
- `{{TOOLS_SCHEMA}}`：注入 `tools_schema.json` 序列化字符串。
- `{{USER_INPUT}}`：注入当前用户输入。

## 四、新增字段统一规则（后续必须遵守）

- 任何新增工具字段或约束，先改 `tools_schema.json`，再改代码。
- 未在 `tools_schema.json` 声明的字段，一律视为非法字段。
- 任何字段变更必须同步更新：
- `SKILL.md`（说明层）
- `prompt.md`（提示层）
- Runtime 校验/映射逻辑（实现层）

## 五、面试可讲亮点（可直接复述）

- 从“规则清单式 Prompt”升级到“SOP 工作流 Prompt”，提升复杂请求稳定性。
- 使用 `tool_name -> args schema` 强绑定，彻底压制 `args` 漂移问题。
- 通过 `additionalProperties=false` 实现强契约 Fail-Fast。
- 架构上实现“文档先行 + 运行时注入 + 代码校验兜底”的三层防线。
- 保障可审计性：提示词、Schema、代码行为可一一对照追溯。
