# StageA-SliceA2 文档契约补齐（2026-02-15）

- 日期：2026-02-15
- 切片编号：StageA-SliceA2
- 测试人：Codex

## 1. 测试目标

- 根据评审意见补齐文档中缺失的三项关键约束：
  - Triangle Count 提取路径与证据字段；
  - Rule Registry 可扩展规则机制；
  - 合成数据中的性能陷阱定义。

## 2. 影响范围

- `Source/HCIEditorGen/文档/01_技术方案_插件双模块.md`
- `Source/HCIEditorGen/文档/03_Schema_v1与错误契约.md`
- `Source/HCIEditorGen/文档/05_开发执行总方案_资产审计.md`
- `Source/HCIEditorGen/文档/06_合成数据生成算法说明.md`
- `SourceData/AbilityKits/Python/hci_generate_synthetic_assets.py`
- `SourceData/AbilityKits/Python/tests/test_hci_generate_synthetic_assets.py`

## 3. 前置条件

- uv Python 3.11.14 可用。

## 4. 操作步骤

1. 按评审项补齐上述文档和脚本。
2. 运行单测：
   - `python -m unittest -v SourceData/AbilityKits/Python/tests/test_hci_generate_synthetic_assets.py`
3. 运行小规模生成验证：
   - `uv run ... hci_generate_synthetic_assets.py --count 120 ...`
4. 检查报告包含 `performance_trap_count/high_triangle_asset_count`。

## 5. 预期结果

- 文档明确面数提取优先级、RuleRegistry 接口、性能陷阱定义。
- 单测通过。
- 生成报告包含性能陷阱统计字段。

## 6. 实际结果

- 文档已补齐对应约束。
- 单测通过（4/4）。
- 小规模生成日志：
  - `Generated 120 assets ... (semantic_gap=36, trap=14, performance_trap=7).`
- 报告包含：
  - `performance_trap_count=7`
  - `high_triangle_asset_count=7`

## 7. 结论

- `Pass`

## 8. 证据

- 文档定位：
  - `Source/HCIEditorGen/文档/03_Schema_v1与错误契约.md`（面数提取契约、Rule Registry 契约）
  - `Source/HCIEditorGen/文档/05_开发执行总方案_资产审计.md`（A2/B4/C1/C3/D1）
  - `Source/HCIEditorGen/文档/06_合成数据生成算法说明.md`（性能陷阱定义）
- 命令输出：
  - 单测通过（4/4）
  - 小规模生成完成（120 条）

## 9. 问题与后续动作

- 进入 StageB 前，需实现 Runtime 侧 `IHCIAbilityAuditRule` 与 `RuleRegistry` 最小骨架。
