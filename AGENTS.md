# HCIAbilityKit Collaboration Contract (V2)

Scope: whole repo.

## 1. 产品定位与目标

- 聚焦单一模块：`HCIAbilityKit`。
- 解决三个核心痛点：策划填表难、资产检索难、数据校验累。
- 核心理念：AI 不是目的，AI 只服务于编辑器工具效率与数据质量。

## 2. 架构基线（冻结）

- 固定采用插件双模块：
  - `HCIAbilityKitRuntime`：数据定义、解析与校验服务。
  - `HCIAbilityKitEditor`：Factory/Reimport、菜单与交互工具。
- 依赖方向必须单向：`Editor -> Runtime`。
- 设计原则：接口简洁、实现深入、依赖抽象、高内聚低耦合。

## 3. 智能中介者工作流（冻结）

- 采用三段式：`计划 -> 执行 -> 审阅`。
- A 层（语义路由）：`Skill(.md + .py)` 轻量调度。
- B 层（执行审计）：生成者与审计者串行复核。
- C 层（交互审阅）：Diff 风格逐项采纳，非黑盒覆盖。
- 敏感操作（修改/删除/批量写入）必须先给出原因并走授权门禁。

## 4. 开发节奏与门禁

- 每个切片开发完成后必须停下，等待用户在 UE 手测。
- 用户未明确 Pass，不得进入下一个切片。
- 重构优先级：先结构迁移，再服务抽离，再 AI 注入，再 UI 升级。

## 5. 测试与数据留存

- Source of Truth：`SourceData/AbilityKits/`。
- 测试记录：`Source/HCIEditorGen/文档/测试记录/`。
- 每次测试必须记录：目标、步骤、预期、结果、证据、结论。

## 6. 文档治理

- 进度唯一权威：`Source/HCIEditorGen/文档/00_总进度.md`。
- 方案唯一权威：`Source/HCIEditorGen/文档/01_技术方案_插件双模块.md`。
- 代码结构发生变化（新增/删除/移动关键代码文件）时，必须同步更新：`Source/HCIEditorGen/文档/04_代码框架总览_树状图.md`（树内中文注释需与实际一致）。
- 任何范围/里程碑变化，先改文档再改代码。
- 保持文档轻量，不扩散到无关主题。

## 7. 质量红线

- Runtime 禁止依赖 Editor-only 模块。
- 失败路径禁止脏写和半覆盖资产。
- 错误信息必须可定位（文件/字段/原因/建议）。
- 未通过验证的结论不得标记为“完成”。

## 8. 当前进度快照（2026-02-12）

- Step 1（结构迁移）已关闭：
  - Slice1：插件双模块骨架落地并通过。
  - Slice2：资产/Factory/Reimport 迁移到插件并通过。
  - Slice3：项目本体 Editor 空壳模块下线并通过。
  - Slice4：Reimport 失败通知可视化（含错误原因）并通过。
- Step 2（服务抽离）进行中：
  - Slice1：JSON 解析从 `UHCIAbilityKitFactory` 抽离到 Runtime `FHCIAbilityKitParserService`，通过。
  - Slice2：打通 `Factory -> Runtime Service -> UE Python` 最小链路，受控失败可见（`__force_python_fail__`），通过。
- 当前下一切片：`Step2-Slice3`（计划让 Python 产出参与字段映射，先从 `DisplayName` 单字段开始）。

## 9. 用户协作习惯（固定）

- 优先在聊天中直接给“下一步操作清单”，不要让用户先翻文档再执行。
- 每个切片必须小步推进；完成后立即停下，等待用户在 UE 手测。
- UE 内手工操作由用户执行；代码改动、构建验证、文档回写由助手执行。
- 用户用 `Pass/Fail` 作为门禁信号；未收到 Pass 不得进入下一切片。
- 回归步骤要具体可执行（短步骤、可复现、可定位失败原因）。
- 继续坚持“接口简单、深度功能、依赖抽象、高内聚低耦合”。
