# 2026-02-25 StageI-SliceI6 AgentChatUI 聊天入口

## 1. 目标

- 在 UE 内提供聊天式自然语言入口，减少长控制台命令输入成本。
- 复用真实 LLM 规划链路与既有 `PlanPreviewUI`，不改变执行语义。
- 提供最小实时反馈：发送中、失败原因、完成摘要。

## 2. 变更范围（冻结）

- `Plugins/HCIAbilityKit/.../UI/`（新增聊天窗口）
- `Plugins/HCIAbilityKit/.../Commands/`（新增打开聊天窗口命令与请求桥接）
- `Source/HCIEditorGen/文档/00_总进度.md`
- `Source/HCIEditorGen/文档/05_开发执行总方案_资产审计.md`

## 3. 实施步骤

1. 文档冻结 I6 目标/边界/门禁。
2. 实现聊天窗口与发送逻辑（复用真实 LLM 规划链路）。
3. 本地编译通过。
4. 给出 UE 手测步骤，等待用户门禁结果。

## 4. UE 手测门禁

1. 控制台输入：`HCIAbilityKit.AgentChatUI`
2. 聊天窗口输入：`帮我看看那个MNew文件夹里有什么`，点击发送
3. 预期：
   - 日志出现 `source=AgentChatUI` 的 `dispatched`
   - 自动打开 `PlanPreview` 窗口
4. 聊天窗口输入：`整理临时目录资产并归档`，点击发送
5. 预期：
   - 聊天窗口出现完成摘要（`intent/route_reason/steps`）
   - 自动打开 `PlanPreview` 窗口

## 5. 结果

- 本地实现：完成
- 本地验证：通过
  - 编译：
    - `Build.bat HCIEditorGenEditor Win64 Development ... -NoHotReloadFromIDE` 通过（exit code 0）
- UE 手测：通过
  - 证据 1：
    - `source=AgentChatUI` + `dispatched` 命中；
    - `intent=inspect_folder_contents`，`steps=2`（`SearchPath -> ScanAssets`）；
    - `AgentPlanPreviewUI opened ... source=AgentChatUI`。
  - 证据 2：
    - `source=AgentChatUI` + `dispatched` 再次命中；
    - `AgentPlanPreviewUI opened ... source=AgentChatUI`；
    - 该次出现 `fallback_reason=llm_contract_invalid`，但 UI 入口链路与回退收敛符合当前主线语义。
- 结论：Pass（I6 关闭）
