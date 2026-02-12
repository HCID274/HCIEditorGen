# HCIAbilityKit Schema v1 与错误契约

> 状态：冻结（M0-V2）
> 版本：v2
> 更新时间：2026-02-12

## 1. 数据输入契约（SourceData）

- 扩展名：`.hciabilitykit`
- 编码：UTF-8（无 BOM）
- Source of Truth：`SourceData/AbilityKits/`

```json
{
  "schema_version": 1,
  "id": "fire_01",
  "display_name": "Fire Ball",
  "params": {
    "damage": 120.0
  }
}
```

- `schema_version`：固定 `1`
- `id`：非空、全局唯一，建议 `[a-z0-9_]+`
- `display_name`：字符串，可空但建议可读
- `params.damage`：数值，建议范围 `0.0 ~ 100000.0`

## 2. 智能调度契约（Skill）

- Skill 文档：`.md`，定义意图、输入输出、风险等级。
- Skill 脚本：`.py`，定义可执行逻辑与返回结构。
- 调度输出最小结构：

```json
{
  "intent": "import_or_reimport",
  "skill_id": "abilitykit_import_v1",
  "risk_level": "medium",
  "requires_permission": true
}
```

## 3. 执行与审计契约（生成者/审计者）

- 生成者输出：候选变更清单（字段、旧值、新值、理由）。
- 审计者输出：可疑项、风险等级、处理建议。
- 审计通过前，不允许落盘修改资产。

```json
{
  "proposals": [
    {
      "field": "params.damage",
      "old": 100.0,
      "new": 120.0,
      "reason": "source_data_update"
    }
  ],
  "audit": {
    "status": "warn",
    "issues": [
      "damage rises by 20%, confirm gameplay balance"
    ]
  }
}
```

## 4. 错误契约（统一格式）

- 输出模板：`[HCIAbilityKit][Error] code=<code> file=<file> field=<field> reason=<reason> hint=<hint>`
- 必填字段：`code/file/field/reason`
- 禁止行为：失败后脏写、半写、无提示静默失败

## 5. 错误码（v2）

- `E1001`：JSON 格式非法
- `E1002`：缺失必填字段
- `E1003`：字段类型错误
- `E1004`：字段值越界或非法
- `E1005`：源文件不存在或不可读
- `E2001`：Skill 未命中或命中冲突
- `E2002`：敏感操作未获授权
- `E3001`：审计未通过（高风险）

## 6. 测试样例（固定）

- `TC_OK_01`：合法导入与重导
- `TC_FAIL_01`：删除 `id`（期望 `E1002`）
- `TC_FAIL_02`：`damage: "abc"`（期望 `E1003`）
- `TC_FAIL_03`：源文件失效（期望 `E1005`）
- `TC_FAIL_04`：敏感操作拒绝授权（期望 `E2002`）

## 7. 演进规则

- 仅允许向后兼容升级（新增字段必须有默认值）。
- `schema_version` 升级时，必须提供迁移策略。
- 变更契约前，先更新测试样例和门禁文档。
