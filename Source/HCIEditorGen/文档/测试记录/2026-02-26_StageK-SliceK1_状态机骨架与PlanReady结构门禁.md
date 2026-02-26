# StageK-SliceK1 测试记录（状态机骨架 + PlanReady 结构门禁）

- 日期：`2026-02-26`
- 切片：`Stage K - Slice K1`
- 目标：
  - ChatUI 主链路引入 C++ 状态机（只读自动执行 / 写入待确认）
  - 增补 PlanReady 结构性强迫门禁（修改意图必须有写步骤、写工具必须消费管道变量）

## 1. 本地实现与验证（助手完成）

- 代码改动：
  - `UHCIAbilityKitAgentSubsystem` 增加会话状态枚举与状态迁移
  - ChatUI 订阅会话状态并按状态启用 `确认并执行`
  - `PlanValidator` 增加 PlanReady 上下文门禁：
    - `modify_intent_requires_write_step`
    - `pipeline_input_required_for_arg`
  - `ToolRegistry` 为 `SetTextureMaxSize/SetMeshLODGroup.asset_paths` 增加 `bRequiresPipelineInput`
- Prompt/Schema/接口文档同步：
  - `AI可调用接口总表.md`
  - `Skills/H3_AgentPlanner/tools_schema.json`
  - `Skills/H3_AgentPlanner/prompt.md`
- 编译：`Build.bat HCIEditorGenEditor Win64 Development ...` 通过
- 自动化（关键）：
  - `HCIAbilityKit.Editor.AgentChat.StateMachine`（3/3）通过
  - `HCIAbilityKit.Editor.AgentPlanValidation.ModifyIntentRequiresWriteStepAtPlanReady` 通过
  - `HCIAbilityKit.Editor.AgentPlanValidation.WriteArgRequiresPipelineInputAtPlanReady` 通过
  - `HCIAbilityKit.Editor.AgentPlanValidation.WriteArgPipelineInputPassesForScanChain` 通过
  - `HCIAbilityKit.Editor.AgentExecutor.PipelineBypassAfterSearchBlockedWithE4009`（回归）通过

## 2. UE 手测步骤（用户执行）

1. 打开聊天入口
   - `HCIAbilityKit.AgentChatUI`
2. 只读请求（应自动执行）
   - `检查一下 /Game/HCI 目录下的模型面数`
3. 写请求（未确认前不得执行）
   - `把 /Game/__HCI_Auto 下所有 Texture2D 的 Maximum Texture Size 设置为 1024，并执行修改。`

## 3. Pass 判定标准

1. 只读请求命中自动执行，不弹 `PlanPreview` 才能完成主链路。
2. 写请求若计划包含写步骤，则未点击确认前不得出现执行日志。
3. 资产合规写计划必须满足结构门禁：
   - 有 `ScanAssets`
   - 有 `SetTextureMaxSize`
   - `SetTextureMaxSize.args.asset_paths` 使用 `{{step_1_scan.asset_paths}}`

## 4. 实际结果（用户 UE 手测）

- 结果：`Pass`
- 证据摘要：
  - 只读请求命中：
    - `tool_name=ScanMeshTriangleCount`
    - `AgentPlanPreview ... mode=execute ... terminal=completed`
    - `AgentChatStateMachine trigger=auto_read_only branch=auto_read_only run_ok=true`
  - 写请求（第一次）出现 planner 漏步为只读扫描，暴露“修改意图被降级”为扫描的问题（已据此补强结构门禁）。
  - 写请求（补强后）命中：
    - `steps=2`
    - `step_1_scan tool_name=ScanAssets`
    - `step_2_set_max_size tool_name=SetTextureMaxSize risk_level=write requires_confirm=true`
    - `args={"asset_paths":["{{step_1_scan.asset_paths}}"],"max_size":1024}`
    - `prepared ... auto_open=false`（未确认前未执行）

## 5. 结论

- `Stage K-SliceK1 Pass`
- 可进入下一片：`Stage K-SliceK2（ui_presentation 契约 + C++ 文案降级）`

