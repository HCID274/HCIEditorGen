# Stage I-SliceI2 测试记录（本地自动化 + UE 门禁）

- 日期：`2026-02-24`
- 目标：验证 PlanPreviewUI 的 I2 展示优化是否生效（步骤状态、参数预览、定位状态、上下文展示）。
- 范围：`No Semantic Changes`（仅展示层与可观测性增强）。

## 1. 测试步骤

1. 编译验证（RED）  
   - 先新增 `AgentPreviewUI.Rows.*` 测试，再编译，确认在未实现新字段时失败（TDD 红灯）。
2. 实现 UI I2 改造  
   - 新增行状态标签：`pending_inputs/no_locate_target/asset_missing/ready_to_locate`。  
   - 新增参数预览：`args=<compact_json>`。  
   - 新增顶部上下文行：`provider/mode/fallback/route_reason`。  
   - 命令层传递 `PreviewContext` 到窗口。
3. 编译验证（GREEN）  
   - `Build.bat HCIEditorGenEditor Win64 Development ... -NoHotReloadFromIDE`。
4. 自动化测试  
   - `Automation RunTests HCI.Editor.AgentPreviewUI.Rows`（2 条）。

## 2. 预期结果

- 两条测试都通过：  
  - `HCI.Editor.AgentPreviewUI.Rows.PendingStateForPlaceholderStep`  
  - `HCI.Editor.AgentPreviewUI.Rows.AssetMissingStateForInvalidAssetPath`
- 测试完成退出码 `0`。

## 3. 实际结果

- 编译：通过（ExitCode=0）。
- 自动化：通过（2/2 成功，ExitCode=0）。

## 4. 证据

- 日志关键行：
  - `Found 2 automation tests based on 'HCI.Editor.AgentPreviewUI.Rows'`
  - `Result={成功} Name={AssetMissingStateForInvalidAssetPath}`
  - `Result={成功} Name={PendingStateForPlaceholderStep}`
  - `**** TEST COMPLETE. EXIT CODE: 0 ****`
- 日志路径：`Saved/Logs/HCIEditorGen.log`

## 5. 结论

- 本地自动化验证通过，进入 UE 手测门禁。  
- 同日 UE 手测已通过，`Stage I-SliceI2` 门禁关闭。

## 6. 追加热修（同日）

- 触发背景：UE 手测日志出现 `SearchPath/ScanAssets` 顺序反转（消费步骤先于来源步骤）。
- 修复动作：
  - Planner 增加变量依赖拓扑归一化（若存在 `{{step_x.*}}` 依赖，自动重排步骤顺序）。
  - PlanValidator 增加顺序校验（来源步骤必须先于消费步骤），违规返回 `E4009/variable_source_step_must_precede_consumer`。
- 回归验证：
  - 编译：`Build.bat ...` 通过（ExitCode=0）。
  - 新增自动化：
    - `HCI.Editor.AgentPlanValidation.VariableSourceMustPrecedeConsumer` 通过。
  - 既有自动化：
    - `HCI.Editor.AgentPreviewUI.Rows` 2 条继续通过。

## 7. UE 手测门禁（通过）

- 手测命令：
  - `HCI.AgentPlanPreviewUI "帮我看看那个MNew文件夹里有什么"`
  - `HCI.AgentPlanPreviewUI "整理临时目录资产并归档"`
- 关键证据：
  - 两条链路均 `provider=llm provider_mode=real_http fallback_used=false plan_validation=ok`。
  - 第一条命中 `route_reason=directory_discovery_search_first`，步骤顺序为：
    - `row=0 step_1_search tool=SearchPath`
    - `row=1 step_2_scan tool=ScanAssets directory={{step_1_search.matched_directories[0]}}`
  - 第二条命中 `route_reason=...step_order_normalized`，步骤顺序已被稳定归一化为：
    - `row=0 step_1_search tool=SearchPath`
    - `row=1 step_2_scan tool=ScanAssets directory={{step_1_search.matched_directories[0]}}`
- 门禁结论：`Pass`。
