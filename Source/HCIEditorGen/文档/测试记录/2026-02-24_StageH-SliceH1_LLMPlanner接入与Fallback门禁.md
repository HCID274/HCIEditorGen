# 2026-02-24 StageH-SliceH1 LLM Planner 接入与 Fallback 门禁

- 日期：2026-02-24
- 切片编号：`Stage H-SliceH1`
- 测试人：用户（UE 手测）

## 1. 测试目标

- 验证 `LLM -> Plan JSON` 生成链路可用。
- 验证 `LLM 失败 -> KeywordFallback` 回退链路可观测。
- 验证生成结果继续经过 `PlanValidator`，且不触发真实写入。

## 2. 影响范围

- Runtime: `FHCIAbilityKitAgentPlanner`（LLM preferred + fallback metadata）。
- Editor: `HCIAbilityKit.AgentPlanWithLLMDemo` / `HCIAbilityKit.AgentPlanWithLLMFallbackDemo`。

## 3. 前置条件

- 工程编译成功并可进入 UE Editor。
- 控制台命令已可执行。

## 4. 操作步骤

1. 执行：`HCIAbilityKit.AgentPlanWithLLMDemo "整理临时目录资产并归档"`
2. 执行：`HCIAbilityKit.AgentPlanWithLLMDemo "扫描关卡里缺碰撞和默认材质"`
3. 执行：`HCIAbilityKit.AgentPlanWithLLMDemo "把高面数网格做LOD检查"`
4. 执行：`HCIAbilityKit.AgentPlanWithLLMFallbackDemo timeout`
5. 执行：`HCIAbilityKit.AgentPlanWithLLMFallbackDemo invalid_json`
6. 执行：`HCIAbilityKit.AgentPlanWithLLMFallbackDemo empty`

## 5. 预期结果

- 成功链路命中：`provider=llm`、`fallback_used=false`、`plan_validation=ok`。
- 回退链路命中：`provider=keyword_fallback`、`fallback_used=true`，且 `fallback_reason` 与注入模式一致。
- 回退错误码命中：`timeout->E4301`、`invalid_json->E4302`、`empty->E4304`。

## 6. 实际结果

- 正常资产归档：`provider=llm`，`intent=normalize_temp_assets_by_metadata`，`PASS`。
- 正常关卡扫描：`provider=llm`，`intent=scan_level_mesh_risks`，`PASS`。
- 正常 LOD 检查：`provider=llm`，`intent=batch_fix_asset_compliance`，`PASS`。
- 超时 Fallback：`provider=keyword_fallback`，`fallback_reason=llm_timeout`，`error_code=E4301`，`PASS`。
- 损坏 JSON Fallback：`provider=keyword_fallback`，`fallback_reason=llm_invalid_json`，`error_code=E4302`，`PASS`。
- 空回复 Fallback：`provider=keyword_fallback`，`fallback_reason=llm_empty_response`，`error_code=E4304`，`PASS`。

## 7. 结论

- `Pass`

## 8. 证据

- 日志路径：UE OutputLog（用户手测回传）
- 截图路径：无
- 录屏路径：无

## 9. 问题与后续动作

- 本切片无阻塞问题，按流程等待用户指定下一切片（`H2` 或 `I1`）。
