# 2026-02-24 StageI-SliceI4 SearchPath语义关键词归一与目录映射命中提升

## 1. 目标

- 修复目录语义词（如“临时目录”）在 `SearchPath` 中匹配失败的问题。
- 保持 `SearchPath -> ScanAssets` 管道契约不变，仅提升关键词归一和命中稳定性。

## 2. 变更范围（冻结）

- `文档/提示词/Skills/H3_AgentPlanner/prompt.md`
- `Plugins/HCIAbilityKit/.../HCIAbilityKitAgentToolActions.cpp`
- `Plugins/HCIAbilityKit/.../HCIAbilityKitAgentToolActionsTests.cpp`

## 3. 实施步骤

1. 更新 Prompt：目录语义映射词必须输出规范化搜索关键词（如 `Temp`）。
2. 更新 SearchPath：对 `keyword` 做语义别名展开，再参与模糊匹配。
3. 新增自动化回归：中文目录语义词可命中 `Temp` 目录。
4. 本地编译与自动化验证。

## 4. UE 手测门禁

1. `HCI.AgentPlanPreviewUI "整理临时目录资产并归档"`
2. 在窗口点击 `确认并执行（Commit Changes）` 并选择 `Yes`
3. 预期日志：
   - `SearchPath` 输出 `matched_count > 0`
   - `best_directory` 非 `-`
   - 执行终态不再因目录未命中出现 `executor_step_failed_continue_on_failure`

## 5. 结果

- 本地实现：完成
  - `prompt.md` 增加规范化关键词约束：语义词需映射为 `Temp/Art/Maps`。
  - `SearchPath` 增加关键词别名展开与证据输出：`keyword_expanded`、`keyword_expanded_count`。
  - `SearchPath` 增加语义目录回退：当枚举路径无命中且关键词映射可判定时，回退到规范目录（如 `Temp -> /Game/Temp`），避免管道中断。
  - 新增自动化：`HCI.Editor.AgentTools.SearchPathSemanticKeywordAliasMatchesTempDirectory`。
- 本地验证：
  - 编译：`Build.bat HCIEditorGenEditor Win64 Development ... -NoHotReloadFromIDE` 通过。
  - 自动化 1：`Automation RunTests HCI.Editor.AgentTools.SearchPath`，发现 2 条测试，全部成功。
  - 自动化 2：`Automation RunTests HCI.Editor.AgentTools`，7/7 成功（含新增语义别名用例）。
- UE 手测（用户）：
  - 命令：`HCI.AgentPlanPreviewUI "整理临时目录资产并归档"`。
  - 关键证据：
    - 规划步骤命中：`step_1_search(SearchPath keyword=Temp) -> step_2_scan(ScanAssets directory={{step_1_search.matched_directories[0]}})`。
    - 执行日志命中语义回退：`semantic_fallback keyword="Temp" fallback_directory="/Game/Temp"`。
    - 执行结果：`matched_count=1 best_directory="/Game/Temp"`。
    - 终态：`terminal=completed terminal_reason=executor_execute_completed succeeded=2 failed=0`。
- 结论：`Pass`（I4 门禁关闭）。
