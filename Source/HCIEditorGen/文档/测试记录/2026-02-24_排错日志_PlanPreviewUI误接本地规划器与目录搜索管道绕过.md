# 2026-02-24 排错日志：PlanPreviewUI 误接本地规划器与目录搜索管道绕过

## 1. 现象

- 用户执行 `HCIAbilityKit.AgentPlanPreviewUI` 时，日志缺失：
  - `[HCIAbilityKit][AgentPlanLLM][H3] dispatched ...`
- 计划直接出现 `route_reason=fallback_scan_assets`，只产出 `ScanAssets` 单步骤。
- 结果：`SearchPath` 新逻辑未被调用，`MNew -> M_New` 修复失效于业务链路层。

## 2. 根因定位（按证据链）

1. 命令入口排查发现：
   - `HCI_RunAbilityKitAgentPlanPreviewUiCommand` 调用的是本地 `HCI_BuildAgentPlanDemoPlan(...)`。
2. `HCI_BuildAgentPlanDemoPlan` 内部走：
   - `FHCIAbilityKitAgentPlanner::BuildPlanFromNaturalLanguage(...)`（本地关键词/规则规划）
3. 该链路不会进入真实 HTTP Provider，因此不会出现 H3 dispatched，也不会读到实时 LLM 规划输出。

结论：这是“命令挂错链路”的集成问题，不是 SearchPath 算法本身的问题。

## 3. 二级根因（为什么仍会 fallback_scan_assets）

- Prompt 中原有规则把 `SearchPath` 写成了可选（`MAY`），同时 `fallback_scan_assets` 触发条件过宽。
- 在目录语义模糊时，LLM 会合法地退化到单步 `ScanAssets`，绕开搜索管道。

## 4. 修复动作

### 4.1 命令链路修复（C++）

- 将 `AgentPlanPreviewUI` 改为：
  - `BuildPlanFromNaturalLanguageWithProviderAsync(...)`
  - 真实 LLM 选项强制对齐 H3：
    - `bPreferLlm=true`
    - `bUseRealHttpProvider=true`
    - `bLlmEnableThinking=false`
    - `bLlmStream=false`
- 结果：UI 命令在弹窗前必须经过真实 LLM 异步等待过程。

### 4.2 搜索匹配修复（C++）

- `SearchPath` 对比前统一清洗：
  - `ToLower` + 去除 `_`、`-`、空格
- 覆盖 `MNew -> M_New` 场景。
- 增加匹配 Debug 日志，输出 keyword、命中 path、score。

### 4.3 路由约束修复（Prompt）

- 新增铁律：目录类非 `/Game/...` 绝对路径请求，第一步必须 `SearchPath`。
- 禁止该类请求走单步 `fallback_scan_assets`。
- 强制 `ScanAssets` 使用 `{{step_1_search.matched_directories[0]}}`。

## 5. 验证结果

- 编译：`HCIEditorGenEditor` 通过（Win64 Development）。
- UE 手测：用户反馈 `完美`，切片通过。

## 6. 面试讲法（可直接复述）

1. 先分层定位：日志是否出现 `H3 dispatched`，快速判断是否走到真实 LLM 链。
2. 再查命令入口：确认 UI 命令是否误接本地 Demo 规划器。
3. 之后收敛两类问题：
   - 集成链路错接（命令层）
   - 策略约束过松（Prompt 层）
4. 最终形成闭环：真实 LLM 接入 + 搜索容错 + 管道路由强约束。
