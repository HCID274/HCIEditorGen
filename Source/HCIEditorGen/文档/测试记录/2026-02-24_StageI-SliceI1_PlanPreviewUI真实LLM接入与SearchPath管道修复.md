# 2026-02-24 StageI-SliceI1 PlanPreviewUI 真实 LLM 接入与 SearchPath 管道修复

- 日期：`2026-02-24`
- 切片编号：`Stage I-SliceI1`
- 测试人：`用户 UE 手测`

## 1. 测试目标

- 验证 `HCIAbilityKit.AgentPlanPreviewUI` 必须走真实 LLM 异步链路，而非本地 Demo 规划器。
- 验证目录模糊词可触发搜索发现链路：`MNew` 能命中实际目录 `M_New`。
- 验证目录类请求在 Prompt 约束下走 `SearchPath -> ScanAssets` 管道，不再单步 `fallback_scan_assets`。

## 2. 影响范围

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Commands/HCIAbilityKitAgentDemoConsoleCommands.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/AgentActions/HCIAbilityKitAgentToolActions.cpp`
- `Source/HCIEditorGen/文档/提示词/Skills/H3_AgentPlanner/prompt.md`

## 3. 前置条件

- `Saved/HCIAbilityKit/Config/llm_provider.local.json` 配置有效。
- UE 工程可编译：`HCIEditorGenEditor Win64 Development`。

## 4. 操作步骤

1. 执行命令：`HCIAbilityKit.AgentPlanPreviewUI "帮我看看那个MNew文件夹里有什么"`。
2. 观察是否先输出真实 LLM 分发日志：`[HCIAbilityKit][AgentPlanLLM][H3] dispatched ... source=AgentPlanPreviewUI`。
3. 观察计划是否走目录搜索管道（非单步 fallback）。
4. 观察 UI 是否在异步结果返回后再打开。

## 5. 预期结果

- `AgentPlanPreviewUI` 不再直接调用本地关键词规划器。
- 目录类非 `/Game/...` 请求必须优先 `SearchPath`。
- `SearchPath` 能匹配 `MNew -> M_New` 并返回完整目录路径。
- 计划通过 `PlanValidator`，UI 正常展示。

## 6. 实际结果

- 命令链路已切到真实 LLM 异步 Provider，且新增 `source=AgentPlanPreviewUI` 的 dispatched 日志。
- `SearchPath` 模糊匹配已修复（目录名统一小写并去掉 `_/-/空格`）。
- Prompt 已新增铁律，目录类请求禁止单步 `fallback_scan_assets`，强制搜索管道。
- 用户验收反馈：`完美`（Pass）。

## 7. 结论

- `Pass`

## 8. 证据

- 代码证据：
  - `.../HCIAbilityKitAgentDemoConsoleCommands.cpp`（`AgentPlanPreviewUI` 改为 `BuildPlanFromNaturalLanguageWithProviderAsync`）
  - `.../HCIAbilityKitAgentToolActions.cpp`（`SearchPath` 遍历与容错匹配修复）
  - `.../提示词/Skills/H3_AgentPlanner/prompt.md`（目录搜索铁律）
- 编译证据：
  - `Build.bat HCIEditorGenEditor Win64 Development ... -NoHotReloadFromIDE` 通过（exit code 0）

## 9. 问题与后续动作

- 下切片建议：`I2`（UI 证据链与步骤状态可视化，强化搜索/扫描/变量管道可观测性）。
