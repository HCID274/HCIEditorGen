# StageK-SliceK2 测试记录（ui_presentation 契约 + C++ 文案降级）

- 日期：`2026-02-26`
- 切片：`Stage K - Slice K2`
- 目标：
  - Planner Step 契约增加可选 `ui_presentation`
  - ChatUI 计划卡主展示切换为人话文案，并提供 C++ fallback（缺失 `ui_presentation` 不崩溃）

## 1. 本地实现与验证（助手完成）

- 文档先行同步：
  - `AI可调用接口总表.md` 增加 `ui_presentation` 字段定义
  - `Skills/H3_AgentPlanner/tools_schema.json` 增加 Step 级 `ui_presentation` schema（可选）
  - `Skills/H3_AgentPlanner/prompt.md` 增加 `ui_presentation` 生成约束（`SHOULD`，至少 `step_summary`）
- Runtime 实现：
  - `FHCIAbilityKitAgentPlanStep` 增加 `UiPresentation`
  - Planner 解析 `ui_presentation`（可选；类型错误按契约无效处理）
  - Validator 增加字段级校验（空值/长度）
- Editor 实现：
  - `UHCIAbilityKitAgentSubsystem` 增加 `BuildStepDisplaySummaryForUi / BuildStepIntentReasonForUi / BuildStepRiskWarningForUi`
  - `BuildLastPlanCardLines` 改为人话主展示，保留 `step_id/risk` 元信息
- 编译：`Build.bat HCIEditorGenEditor Win64 Development ...` 通过
- 自动化（新增用例已编译通过，本轮未在 UE 内执行）：
  - `HCI.Editor.AgentPlanValidation.UiPresentationEmptySummaryRejected`
  - `HCI.Editor.AgentPlanValidation.UiPresentationRiskWarningTooLongRejected`
  - `HCI.Editor.AgentChat.UiPresentation.StepSummaryPreferredOverFallback`
  - `HCI.Editor.AgentChat.UiPresentation.MissingSummaryFallsBackToCppHumanText`

## 2. UE 手测步骤（用户执行）

1. 打开聊天入口
   - `HCI.AgentChatUI`
2. 只读风险扫描（验证人话步骤）
   - `扫描缺碰撞和默认材质问题`
3. 写请求（验证风险提示 + 未确认不执行）
   - `把 /Game/__HCI_Auto 下所有 Texture2D 的 Maximum Texture Size 设置为 1024，并执行修改。`

## 3. Pass 判定标准

1. 计划卡步骤主展示为中文人话摘要，不以 `tool_name + args` 作为主文案。
2. 写步骤在计划卡可展示风险提示（来自 `ui_presentation` 或 C++ fallback）。
3. K1 语义不回归：写请求未确认前不得执行真实写操作。

## 4. 实际结果（用户 UE 手测）

- 结果：`Pass`
- 证据摘要：
  - 用户确认 K2 门禁通过（`Pass!`）。
  - 风险扫描请求：计划卡已显示人话步骤摘要（符合“非 JSON 主展示”目标）。
  - 贴图修改请求：计划卡显示风险提示，且未确认前不执行写操作（沿用 K1 门禁行为）。

## 5. 结论

- `Stage K-SliceK2 Pass`
- 可进入下一片：`Stage K-SliceK3（聊天内审查卡：取消/确认执行）`

