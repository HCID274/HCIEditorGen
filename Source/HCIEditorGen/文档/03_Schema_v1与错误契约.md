# HCIAbilityKit Schema v1 与错误契约

> 状态：冻结（M0-V5）
> 更新时间：2026-02-26

## 1. SourceData 输入契约

- 扩展名：`.hciabilitykit`
- 编码：UTF-8（无 BOM）
- Source of Truth：`SourceData/AbilityKits/`

```json
{
  "schema_version": 1,
  "id": "fire_01",
  "display_name": "Fire Ball",
  "representing_mesh": "/Game/Seed/SM_FireRock_01.SM_FireRock_01",
  "params": {
    "damage": 120.0,
    "triangle_count_lod0": 500
  }
}
```

- 必填：`schema_version/id/params.damage`
- 可选：`display_name/representing_mesh/params.triangle_count_lod0/tags/description`
- `representing_mesh` 必须是 UE 长对象路径。

## 2. Plan JSON 最小契约

```json
{
  "plan_version": 1,
  "request_id": "req_20260224_0001",
  "intent": "normalize_temp_assets_by_metadata",
  "assistant_message": "先给你结论，再继续执行计划。",
  "steps": [
    {
      "step_id": "s1",
      "tool_name": "NormalizeAssetNamingByMetadata",
      "args": {
        "scope": "selected"
      },
      "risk_level": "write",
      "requires_confirm": true,
      "rollback_strategy": "all_or_nothing"
    }
  ]
}
```

- 顶层必填：`plan_version/request_id/intent/steps`
- 顶层可选：`assistant_message`
- 语义规则：
  - `steps=[]` 且 `assistant_message` 非空时，表示“语义直接回复”（不执行工具）。
  - `steps` 非空时，执行链路优先处理步骤；`assistant_message`仅作为前导说明。
- 步骤必填：`step_id/tool_name/args/risk_level/requires_confirm/rollback_strategy`
- `risk_level` 枚举：`read_only/write/destructive`

## 3. H1 LLM Planner 扩展契约（新增）

### 3.1 Planner 请求（内部）

```json
{
  "request_id": "req_20260224_0001",
  "user_prompt": "整理临时目录资产并归档",
  "planner_mode": "llm_preferred"
}
```

### 3.2 Planner 结果（统一）

```json
{
  "request_id": "req_20260224_0001",
  "planner_provider": "llm",
  "fallback_used": false,
  "fallback_reason": "",
  "plan": {"plan_version": 1}
}
```

- `planner_provider` 枚举：`llm|keyword_fallback`
- `fallback_reason` 枚举：`llm_timeout|llm_invalid_json|llm_contract_invalid|llm_empty_response|llm_circuit_open|llm_http_error|llm_config_missing|none`
- H2 观测字段：
  - `llm_attempts`：本次请求 LLM 尝试次数（含重试）。
  - `retry_used`：是否发生重试。
  - `circuit_open`：是否命中熔断打开态。
  - `consecutive_llm_failures`：当前连续 LLM 失败计数。
- 结果必须继续过 `PlanValidator`，失败不得进入执行。

## 4. 错误码（当前）

- 导入/解析：`E1001~E1006`
- Plan/门禁：`E4001~E4012`
- 审阅/桥接：`E4201~E4219`
- H1 新增预留：
  - `E4301`：`llm_request_timeout`
  - `E4302`：`llm_response_invalid_json`
  - `E4303`：`llm_plan_contract_invalid`
  - `E4304`：`llm_empty_response`
- H2 新增：
  - `E4305`：`llm_circuit_open`
- H3 新增：
  - `E4306`：`llm_http_error`
  - `E4307`：`llm_config_missing`

## 5. 不可违反约束

- Runtime 禁止依赖 Editor-only。
- 失败路径禁止脏写和半写。
- 任意回退路径必须可观测（日志需含 `request_id/provider/fallback_reason/error_code`）。
- 任何“完成”结论必须有测试记录证据。

## 6. 演进规则

- 仅向后兼容升级（新增字段必须有默认值）。
- 契约变更先改文档，再改代码。
- 历史桥接明细已迁移到测试记录目录，不再在本文件展开。
