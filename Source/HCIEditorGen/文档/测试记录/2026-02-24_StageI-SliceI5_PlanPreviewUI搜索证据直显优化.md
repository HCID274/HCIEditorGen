# 2026-02-24 StageI-SliceI5 PlanPreviewUI 搜索证据直显优化

## 1. 目标

- 在 `HCIAbilityKit.AgentPlanPreviewUI` 窗口内直接展示 `SearchPath` 语义匹配证据：
  - `keyword`
  - `keyword_normalized`
  - `keyword_expanded`
  - `matched_count`
  - `best_directory`
  - `semantic_fallback_used/semantic_fallback_directory`
- 保持执行链路语义不变，仅提升 UI 可观测性。

## 2. 变更范围（冻结）

- `Plugins/HCIAbilityKit/.../UI/HCIAbilityKitAgentPlanPreviewWindow.h`
- `Plugins/HCIAbilityKit/.../UI/HCIAbilityKitAgentPlanPreviewWindow.cpp`
- `Plugins/HCIAbilityKit/.../Tests/HCIAbilityKitAgentPlanPreviewWindowTests.cpp`

## 3. 实施步骤

1. 先新增自动化测试（Red）：覆盖 SearchPath 证据摘要构建行为。
2. 实现 UI 证据摘要构建与展示（Green）。
3. 回归编译与自动化测试（Refactor 保持绿灯）。
4. 提供 UE 手测命令与验收口径，等待用户门禁 Pass。

## 4. UE 手测门禁

1. `HCIAbilityKit.AgentPlanPreviewUI "整理临时目录资产并归档"`
2. 在窗口点击 `确认并执行（Commit Changes）`，选择 `Yes`
3. 预期窗口内可见 SearchPath 证据摘要，至少包含：
   - `best_directory=/Game/Temp`
   - `semantic_fallback=true`
   - `keyword_expanded=Temp`

## 5. 结果

- 本地实现：完成
  - `PlanPreviewWindow` 新增 `SearchPath` 证据摘要展示行（执行后实时刷新）。
  - 证据摘要字段：`keyword/normalized/expanded/matched_count/best_directory/semantic_fallback`。
  - 无 `SearchPath` 步骤时显示：`SearchPath证据: 本计划不含 SearchPath 步骤`。
- 本地验证：通过
  - 编译：
    - `Build.bat HCIEditorGenEditor Win64 Development -Project=... -WaitMutex -FromMSBuild -NoHotReloadFromIDE` 通过（exit code 0）。
  - 自动化：
    - `Automation RunTests HCIAbilityKit.Editor.AgentPreviewUI.SearchPathEvidence`：2/2 成功。
    - `Automation RunTests HCIAbilityKit.Editor.AgentPreviewUI`：5/5 成功（含新增 2 条 SearchPath 证据用例）。
  - 说明：
    - 命令行自动化需追加 `-NoDDC`，否则本机会触发 DDC 可写节点异常导致 UE-Cmd 启动失败（与业务代码无关）。
- UE 手测：待执行
- UE 手测：通过
  - 用户门禁反馈：`Pass`
  - 结论：窗口内证据直显路径已满足门禁，I5 关闭。
- 结论：Pass
