# HCIAbilityKit Collaboration Contract (V2)

Scope: whole repo.

## 1. 产品定位与目标

- 聚焦单一模块：`HCIAbilityKit`。
- 解决三个核心痛点：资产规范执行难、性能风险发现慢、数据校验累。
- 核心理念：工具能力服务于编辑器效率与数据质量。
- 功能规划必须对齐真实开发场景（优先资产审计与跨部门协同），禁止仅为演示而做“玩具化功能”。
- 当前交付收束：仅做“资产审计主链路”专精模块；不做无确认自动改写资产与审批/审阅 UI。
- 路线衔接：`B4->D3` 收尾后，进入 `Stage E（Agent安全执行）` 与 `Stage F（指令解析与编排）`。
- 增强项（储备池）允许文档保留，但不得阻塞当前切片门禁。
- 一期冻结决议（2026-02-21）：
  - 深做：`Plan JSON 契约`、`Tool Registry 能力声明`、`Dry-Run Diff + Camera Focus`，并强制覆盖三维业务闭环：`资产规范合规（面数阈值+自动LOD、Texture NPOT+分辨率降级）`、`关卡场景排雷（StaticMeshActor 缺失 Collision/Default Material 检测与定位）`、`命名溯源整理（基于 UAssetImportData/AssetUserData 的自动前缀命名与批量 Move 归档）`。
  - 极简：`MAX_ASSET_MODIFY_LIMIT=50`、`All-or-Nothing`、`SourceControl Fail-Fast`、本地 Mock 鉴权与日志。
  - 延期：记忆门禁/TTL 与线上 KPI（Phase 3，不进入一期代码）。
  - 细则：`args_schema` 必须冻结枚举边界；Actor 用 `Camera Focus`，纯资产用 `SyncBrowserToObjects`；SourceControl 未启用时进入离线本地模式；未命中用户默认 `Guest(read_only)`；LOD 工具必须拦截 Nanite 资产。

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
- Prompt/Skill 边界：一次性低风险分析可直接 Prompt；可复现、可审计、固定流程能力必须走 Skill。

## 4. 开发节奏与门禁

- 每个切片开发完成后必须停下，等待用户在 UE 手测。
- 用户未明确 Pass，不得进入下一个切片。
- 重构优先级：先结构迁移，再服务抽离，再审计主链路完善，再 UI 升级。
- 进度节奏不得拖慢：默认每个切片按“文档冻结 -> 实现 -> UE 手测门禁”短周期推进；若超过约定窗口，必须主动给出阻塞原因与加速方案。
- 当“当前计划规定内容”已完成时，必须立即停下并询问用户下一步动向；禁止助手自行扩展到未冻结范围。

## 5. 测试与数据留存

- Source of Truth：`SourceData/AbilityKits/`。
- 测试记录：`Source/HCIEditorGen/文档/测试记录/`。
- 每次测试必须记录：目标、步骤、预期、结果、证据、结论。

## 6. 文档治理

- 进度唯一权威：`Source/HCIEditorGen/文档/00_总进度.md`。
- 方案唯一权威：`Source/HCIEditorGen/文档/01_技术方案_插件双模块.md`。
- 执行主计划权威：`Source/HCIEditorGen/文档/05_开发执行总方案_资产审计.md`（当前主线切片必须按此顺序推进）。
- 代码结构发生变化（新增/删除/移动关键代码文件）时，必须同步更新：`Source/HCIEditorGen/文档/04_代码框架总览_树状图.md`（树内中文注释需与实际一致）。
- 任何范围/里程碑变化，先改文档再改代码。
- 保持文档轻量，不扩散到无关主题。
- 用户在 UE 手测中明确反馈 `Pass` 后，助手必须主动完成文档收口（至少更新 `00_总进度.md` 与涉及变更的主计划/结构文档）；收口完成后必须停下并等待用户下一步命令。
- Prompt 文档唯一入口：`Source/HCIEditorGen/文档/提示词/README.md`；采用 Skills Bundle 结构（`SKILL.md + prompt.md + tools_schema.json`）。
- Prompt 运行时注入占位符冻结：`{{TOOLS_SCHEMA}}`、`{{USER_INPUT}}`。
- 新增字段/新增工具/约束变更时，必须先更新对应 Skill 的 `tools_schema.json`，再改代码；禁止“代码先行、文档补录”。

## 7. 质量红线

- Runtime 禁止依赖 Editor-only 模块。
- 失败路径禁止脏写和半覆盖资产。
- 错误信息必须可定位（文件/字段/原因/建议）。
- 未通过验证的结论不得标记为“完成”。

## 8. 当前进度快照（精简，2026-02-25）

- 已完成阶段：
  - `Step 1` 结构迁移：完成并通过门禁。
  - `Step 2` 服务抽离：完成并通过门禁。
  - `Stage A~D` 资产审计主链路：完成并通过门禁。
  - `Stage E` Agent 安全执行：完成并通过门禁。
  - `Stage F` 指令解析与执行编排（`F1~F15`）：完成并通过门禁。
  - `Stage G`（`G1~G10`）：完成并通过 UE 手测门禁。
- 当前切片：
  - `Stage H-SliceH1`：已通过 UE 手测。
  - `Stage H-SliceH2`：已通过 UE 手测（稳定性：retry/circuit/metrics）。
  - `Stage H-SliceH3`：已通过 UE 手测（真实 LLM Provider：`C++ + FHttpModule + 本地配置文件`）。
  - `Stage H-SliceH3-Prompt`：已完成 Skills Bundle 化（`H3_AgentPlanner`）并切换 Runtime 读取（`prompt.md + tools_schema.json` 注入）。
  - `Stage H-SliceH4`：已通过 UE 手测（Runtime 透明路由：`<50%` 成功率剔除 + `smooth_wrr` 按到期权重轮询）。
  - `Stage I-SliceI1`：已通过 UE 手测（PlanPreviewUI 真实 LLM 链路 + SearchPath 管道修复）。
  - `Stage I-SliceI2`：已通过 UE 手测（UI 展示优化 + 变量管道顺序归一化）。
  - `Stage I-SliceI3`：已通过 UE 手测（确认并执行真实写操作闭环）。
  - `Stage I-SliceI4`：已通过 UE 手测（SearchPath 语义关键词归一 + 语义目录回退，`Temp -> /Game/Temp`）。
  - `Stage I-SliceI5`：已通过 UE 手测（PlanPreviewUI 直显 `SearchPath` 语义归一与最优目录证据）。
  - `Stage I-SliceI6`：已通过 UE 手测（AgentChatUI 聊天式入口 + 真实 LLM 异步链路复用）。
- UE 手测命令（G10，历史通过记录）：
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecutionReadiness 1 none`
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecutionReadiness 0 none`
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecutionReadiness 1 digest`
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecutionReadiness 1 archive`
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecutionReadiness 1 mode`
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecutionReadinessJson 1 none`
- 最近完成切片：
  - `Phase2-SliceP2-1`：已通过 UE 手测门禁（Commands LLM 处理器拆分，`No Semantic Changes`）。
  - `Phase2-SliceP2-2`：已通过 UE 手测门禁（Commands StageG 处理器拆分，`No Semantic Changes`）。
- 当前切片：已收口，待你指定下一片。
- 下一切片（待你选择）：
  - `Stage I-SliceI7`：聊天 UI 体验增强（先冻结目标与门禁）。
  - 若继续结构解耦：先冻结新增切片目标与门禁，再进入实现。

## 9. 用户协作习惯（固定）

- 优先在聊天中直接给“下一步操作清单”，不要让用户先翻文档再执行。
- 每个切片必须小步推进；完成后立即停下，等待用户在 UE 手测。
- UE 内手工操作由用户执行；代码改动、构建验证、文档回写由助手执行。
- 用户用 `Pass/Fail` 作为门禁信号；未收到 Pass 不得进入下一切片。
- 回归步骤要具体可执行（短步骤、可复现、可定位失败原因）。
- 继续坚持“接口简单、深度功能、依赖抽象、高内聚低耦合”。
- 始终坚持“文档先行”：先定框架（目标边界、契约字段、门禁口径），再在框架内实现；框架外需求必须先更新文档并得到用户确认。
- 若当前切片已按计划完成，助手必须先向用户确认“下一步做哪一片”，再继续执行，不得默认跳片推进。

## 10. 本机构建路径备忘（2026-02-24）

- UE 构建脚本（已验证可执行）：
  - `D:\Unreal Engine\UE_5.4\Engine\Build\BatchFiles\Build.bat`

## 11. Skills 标准（冻结，2026-02-24）

- Skills 必须按 Bundle 管理：`文档/提示词/Skills/<SkillName>/SKILL.md|prompt.md|tools_schema.json`。
- `tools_schema.json` 为参数边界唯一文档源，默认 `additionalProperties=false`。
- Prompt 以 SOP 结构组织，强调“Intent -> Tool -> Args -> JSON”顺序，不得只写松散规则列表。
- 任何后续新增字段，必须遵循同一标准：先 Schema，后 Prompt，再代码实现与测试门禁。

## 12. 防跑偏强制校准（冻结，2026-02-25）

- 跑偏复盘结论（固定口径）：
  - 曾出现“能力总结漏项”，根因是只按当前 UI 已接线能力口头归纳，未同时核对“规划白名单 + 运行时白名单 + 执行接线状态”三层事实源。
  - 结果导致对最终能力上限的描述偏窄（遗漏资产合规主线：`SetTextureMaxSize/SetMeshLODGroup`）。
- 以后每次回答“当前开发到哪/最终上限是什么/下一步做什么”前，必须按以下顺序强制核对：
  1. 产品与范围上限：`AGENTS.md` 第 1 节 + `Source/HCIEditorGen/文档/05_开发执行总方案_资产审计.md`。
  2. Planner 可规划工具面：`Source/HCIEditorGen/文档/提示词/Skills/H3_AgentPlanner/tools_schema.json`。
  3. Runtime 工具白名单：`Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Agent/HCIAbilityKitToolRegistry.cpp`。
  4. 实际 ToolAction 接线：`Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/AgentActions/HCIAbilityKitAgentToolActions.cpp`（`BuildStageIDraftActions`）。
  5. 未接线时的执行收敛：`Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Agent/HCIAbilityKitAgentExecutor.cpp`（`HCI_TryRunToolAction` 与 simulated 分支）。
- 输出要求（强制）：
  - 必须分开描述：`可规划（Planner）`、`白名单（Registry）`、`已实接可执行（ToolAction）`、`未接线将模拟（Executor）`。
  - 禁止将“可规划”直接表述为“已真实执行”。
- AI 接口总表（单一入口）：
  - `Source/HCIEditorGen/文档/提示词/AI可调用接口总表.md`。
  - 任一工具新增/删除/参数变更/执行接线变更时，必须先更新该总表，再改 Skill 与代码。

## 13. API 测试与路由权重更新备忘

- **新增模型/更新测试**：修改 `SourceData/AbilityKits/Python/hci_bailian_model_router.py` 中的 `MODEL_EXPIRIES` 元组。
- **执行测试命令**：直接运行上述 Python 脚本。脚本会自动读取本地配置中的 `api_key` 并发起多线程网络测试。
  - `python SourceData/AbilityKits/Python/hci_bailian_model_router.py`
- **生效机制**：脚本执行成功后，会自动无感覆写 `Saved/HCIAbilityKit/Config/llm_router.local.json`，C++ Runtime 将在下一次 LLM 请求时自动采用新权重。

