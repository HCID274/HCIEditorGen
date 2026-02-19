# StageA-SliceA3 实战资产导入方案冻结（2026-02-15）

- 日期：2026-02-15
- 切片编号：StageA-SliceA3
- 测试人：Codex

## 1. 测试目标

- 将 Stage B 启动方案固定为“种子资产 + RepresentingMesh 引用链 + 真实面数追踪”。
- 明确下一实现起点，避免继续停留在“仅 JSON 数字审计”的伪真实性路径。

## 2. 影响范围

- `AGENTS.md`
- `Source/HCIEditorGen/文档/00_总进度.md`
- `Source/HCIEditorGen/文档/01_技术方案_插件双模块.md`
- `Source/HCIEditorGen/文档/03_Schema_v1与错误契约.md`
- `Source/HCIEditorGen/文档/05_开发执行总方案_资产审计.md`
- `Source/HCIEditorGen/文档/06_合成数据生成算法说明.md`

## 3. 前置条件

- StageA-SliceA2 已通过。
- 文档主线仍遵循“资产审计优先，AI 后置”。

## 4. 操作步骤

1. 在 `05` 中新增 Stage A-SliceA3 与 Stage B `B0~B7` 切片，固化种子资产来源、引用链与拖入场景验证。
2. 在 `03` 冻结 `representing_mesh` 字段与 `E1006` 错误契约，并将其纳入 Triangle 证据链。
3. 在 `01/00/AGENTS` 同步“当前切片/下一切片/职责边界”。
4. 在 `06` 标注生成器的 `seed_mesh_manifest -> representing_mesh` 增强为 Stage B 待实现项。
5. 补充风险收束：Stage D 批次加载与 GC 节流、路径鸿沟边界、actual vs expected 面数冲突规则。

## 5. 预期结果

- 所有权威文档对 Stage B 起点一致：先有真实种子资产，再做扫描和规则。
- `RepresentingMesh` 成为正式契约字段，可用于拖拽预览和真实面数追踪。
- 下一步实现切片可直接执行，无语义歧义。

## 6. 实际结果

- 文档已完成同步更新。
- Stage B 已改为“实战资产导入 + 异步扫描主链路”，新增 `Slice B0/B1/B7`。
- `03` 已补充 `representing_mesh` 字段示例、`E1006` 与 `TC_FAIL_05`。
- `06` 已明确真实网格绑定为 Stage B 增强，不与现状脚本能力冲突。
- 已新增：`TriangleExpectedMismatchRule (Warn)`、`seed_mesh_manifest` UE 长路径格式约束、Factory 校验边界。
- 已新增：预览体自动同步要求（`UStaticMeshComponent + PostEditChangeProperty`）。

## 7. 结论

- `待用户确认 Pass/Fail`

## 8. 证据

- 关键定位：
  - `Source/HCIEditorGen/文档/05_开发执行总方案_资产审计.md`
  - `Source/HCIEditorGen/文档/03_Schema_v1与错误契约.md`
  - `Source/HCIEditorGen/文档/01_技术方案_插件双模块.md`
  - `Source/HCIEditorGen/文档/00_总进度.md`
  - `AGENTS.md`

## 9. 问题与后续动作

- 进入 `Stage B-SliceB0` 前，需你在项目中先准备 `/Game/.../Seed/` 高面数种子模型。
- `seed_mesh_manifest` 生成脚本与 `representing_mesh` 自动分配逻辑在 `Stage B-SliceB0/B1` 实现。
