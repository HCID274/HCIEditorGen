# StageB-SliceB8 预览体自动同步（2026-02-22）

- 日期：2026-02-22
- 切片编号：StageB-SliceB8
- 测试人：用户（UE 手测）+ 助手（实现与编译）

## 1. 测试目标

- 验证 `AHCIAbilityKitPreviewActor` 在以下场景自动刷新网格，不依赖手动点击 `RefreshPreview`：
  - Actor 上 `AbilityAsset` 变更；
  - `UHCIAbilityKitAsset.RepresentingMesh` 在编辑器中被修改；
  - AbilityKit 执行 `Reimport` 后 `RepresentingMesh` 更新。

## 2. 影响范围

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Audit/HCIAbilityKitPreviewActor.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Audit/HCIAbilityKitPreviewActor.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Factories/HCIAbilityKitFactory.cpp`

## 3. 前置条件

- 工程可编译通过（已通过）。
- 项目中至少有 2 个 `UHCIAbilityKitAsset`，并且它们的 `RepresentingMesh` 不同。
- 关卡中可放置 `AHCIAbilityKitPreviewActor`（或其 BP 子类）。

## 4. 操作步骤（UE）

1. 在关卡放置一个 `AHCIAbilityKitPreviewActor`。
2. 给该 Actor 的 `AbilityAsset` 赋值为 `Asset_A`，确认显示 `Asset_A.RepresentingMesh`。
3. 不点击 `RefreshPreview`，直接把 `AbilityAsset` 改为 `Asset_B`，观察网格是否立即切换为 `Asset_B.RepresentingMesh`。
4. 打开 `Asset_B`，在 Details 把 `RepresentingMesh` 改成第三个网格 `Mesh_C` 并保存，观察场景中该 PreviewActor 是否自动更新为 `Mesh_C`。
5. 修改 `Asset_B` 对应的源 `.hciabilitykit` 文件中的 `representing_mesh`，回到内容浏览器右键 `Asset_B -> Reimport`。
6. 观察场景中绑定 `Asset_B` 的 PreviewActor 是否自动切到重导入后的网格。

## 5. 预期结果

- 步骤 3：`AbilityAsset` 切换后网格自动更新。
- 步骤 4：`RepresentingMesh` 修改后网格自动更新。
- 步骤 6：`Reimport` 成功后网格自动更新。
- 全程无需手动点击 `RefreshPreview` 按钮。

## 6. 实际结果

- 用户在聊天门禁反馈：`Pass`。
- 验证结论：`AbilityAsset` 切换、`RepresentingMesh` 变更、`Reimport` 后预览体均可自动刷新；无需手动点击 `RefreshPreview`。

## 7. 结论

- `Pass`

## 8. 证据

- 日志路径：`Saved/Logs/HCIEditorGen.log`
- 关键日志（可选）：`[HCIAbilityKit][PreviewSync] source=import/reimport ... refreshed_actors=...`
- 截图路径：待补
- 录屏路径：待补

## 9. 问题与后续动作

- 若步骤 4/6 未自动更新，优先记录：
  - 资产路径与网格路径；
  - Reimport 成功/失败日志；
  - 是否命中 `PreviewSync` 日志。
