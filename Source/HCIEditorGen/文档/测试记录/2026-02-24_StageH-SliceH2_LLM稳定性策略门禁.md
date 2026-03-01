# 2026-02-24 StageH-SliceH2 LLM 稳定性策略门禁

## 1. 基本信息

- 切片编号：`Stage H-SliceH2`
- 记录时间：`2026-02-24`
- 执行人：`用户 UE 手测`
- 代码状态：`已实现并通过 UE 手测`

## 2. 测试目标

- 验证 `retry` 成功与失败回退路径可观测。
- 验证 `circuit breaker` 打开后阻断 LLM 并回退关键词规划器。
- 验证稳定性指标字段可观测、可复现。

## 3. UE 手测命令

1. `HCI.AgentPlanWithLLMStabilityDemo reset_metrics`
2. `HCI.AgentPlanWithLLMStabilityDemo retry_success "整理临时目录资产并归档"`
3. `HCI.AgentPlanWithLLMStabilityDemo retry_fallback "整理临时目录资产并归档"`
4. `HCI.AgentPlanWithLLMStabilityDemo circuit_open "整理临时目录资产并归档"`
5. `HCI.AgentPlanWithLLMStabilityDemo all`
6. `HCI.AgentPlanWithLLMMetricsDump`

## 4. 预期结果

- `retry_success`：`provider=llm`，`llm_attempts=2`，`retry_used=true`。
- `retry_fallback`：`provider=keyword_fallback`，`fallback_reason=llm_timeout`，`error_code=E4301`。
- `circuit_open`：后续请求命中 `fallback_reason=llm_circuit_open`，`error_code=E4305`。
- 指标输出包含：
  - `total_requests`
  - `llm_preferred_requests`
  - `llm_success_requests`
  - `keyword_fallback_requests`
  - `retry_used_requests`
  - `retry_attempts`
  - `circuit_open_fallback_requests`
  - `consecutive_llm_failures`

## 5. 实测结果

- 指标清理：`reset_metrics=ok, total_requests=0`，`PASS`。
- 重试救回：`llm_attempts=2, retry_used=true, provider=llm`，`PASS`。
- 重试耗尽回退：`fallback_reason=llm_timeout, provider=keyword_fallback`，`PASS`。
- 熔断触发：`llm_attempts=0, reason=llm_circuit_open, error_code=E4305`，`PASS`。
- 综合全量测试：四种状态（`Success/Fallback/Trigger/Open`）切换正确，`PASS`。
- 指标审计：`circuit_open_fallback_requests=1` 等计数器对齐，`PASS`。

## 6. 证据

- 用户回传 UE 控制台/OutputLog 结果（6/6 用例均 `PASS`）。

## 7. 结论

- 结论：`Stage H-SliceH2 Pass`，门禁关闭。
