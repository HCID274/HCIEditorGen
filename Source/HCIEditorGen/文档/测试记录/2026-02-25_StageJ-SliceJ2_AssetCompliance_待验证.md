# Stage J-SliceJ2 测试记录：资产合规自动化（SetTextureMaxSize & SetMeshLODGroup）

- 测试日期：2026-02-25
- 测试人：用户
- 状态：**待验证**

## 1. 测试目标
验证 `SetTextureMaxSize` 和 `SetMeshLODGroup` 工具的接入情况。通过在 Agent 中下达指令，它应该能扫描指定目录的贴图和模型资产，并在点击执行后，**真实修改**这些资产的 `MaxTextureSize` 或 `LODGroup` 属性。

## 2. 测试环境
- 目录：`/Game/Temp` 等可修改的测试目录，里面存放几张贴图（`Texture2D`）或静态网格体（`StaticMesh`）。
- 启动方式：本地 `HCIEditorGenEditor`

## 3. 测试步骤与预期

**步骤 1：触发贴图分辨率限制**
- 操作：打开 UE，执行控制台命令 `HCIAbilityKit.AgentChatUI`。
- 操作：在聊天窗口中输入 `把临时目录里所有的贴图分辨率限制到 1024` 并发送。
- 预期：
  1. 弹出 `PlanPreviewUI`，展示 `SearchPath -> ScanAssets -> SetTextureMaxSize` 计划。
  2. 点击 `确认并执行（Commit Changes）` -> `Yes` 后，Output Log 打印 `set_texture_max_size_execute_ok`，并且能看到 `modified_count` 大于 0。
  3. **验收**：双击被修改的贴图资产，检查 Details 面板中的 `Maximum Texture Size` 是否已变为 1024。

**步骤 2：触发模型 LOD 组修改**
- 操作：在聊天窗口中输入 `把临时目录里所有的模型设置成 LevelArchitecture LOD组` 并发送。
- 预期：
  1. 弹出 `PlanPreviewUI`，展示 `SearchPath -> ScanAssets -> SetMeshLODGroup` 计划。
  2. 点击执行后，Output Log 打印 `set_mesh_lod_group_execute_ok`。
  3. **验收**：双击模型资产，检查 Details 面板中的 `LOD Group` 是否已变为 `LevelArchitecture`。

## 4. 结论与证据
- **结论**：待用户手测反馈（Pass / Fail）。
- **执行证据（日志）**：（待用户测试后补充）