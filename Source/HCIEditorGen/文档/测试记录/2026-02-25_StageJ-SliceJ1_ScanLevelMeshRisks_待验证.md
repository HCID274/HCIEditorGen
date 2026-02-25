# Stage J-SliceJ1 测试记录：关卡场景排雷闭环（ScanLevelMeshRisks）

- 测试日期：2026-02-25
- 测试人：用户
- 状态：**待验证**

## 1. 测试目标
验证 `ScanLevelMeshRisks` 工具是否已正确接入并执行。通过在 Agent 中下达指令，它应该能扫描当前关卡内的 `StaticMeshActor`，正确筛选出“缺失 Collision”或“使用了 Default Material”的风险对象。

## 2. 测试环境
- 关卡：任意包含 `StaticMeshActor` 的测试关卡（可故意摆放几个缺失碰撞和使用默认材质的模型）。
- 启动方式：本地 `HCIEditorGenEditor`

## 3. 测试步骤与预期

**步骤 1：触发场景扫描计划**
- 操作：打开 UE，执行控制台命令 `HCIAbilityKit.AgentChatUI`。
- 操作：在弹出的聊天窗口中输入 `扫描关卡里缺碰撞和默认材质` 并发送。
- 预期：
  1. 系统成功完成 LLM 规划，自动弹出 `PlanPreviewUI` 窗口。
  2. 预览窗口中存在工具为 `ScanLevelMeshRisks` 的计划步骤。

**步骤 2：执行扫描操作**
- 操作：在 `PlanPreviewUI` 窗口点击 `确认并执行（Commit Changes）`，在弹出的高危操作确认框选择 `Yes`。
- 预期：
  1. 窗口执行成功，并显示扫描摘要。
  2. Output Log 中应当可见包含风险 Actor 名字的 `risky_actors` 证据（并标注原因是 `[MissingCollision]` 还是 `[DefaultMaterial]`）。

## 4. 结论与证据
- **结论**：待用户手测反馈（Pass / Fail）。
- **执行证据（日志）**：（待用户测试后补充）
