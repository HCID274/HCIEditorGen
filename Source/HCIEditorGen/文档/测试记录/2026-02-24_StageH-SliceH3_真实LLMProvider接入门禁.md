# 2026-02-24 StageH-SliceH3 真实 LLM Provider 接入门禁

## 1. 基本信息

- 切片编号：`Stage H-SliceH3`
- 记录时间：`2026-02-24`
- 执行人：`用户 UE 手测`
- 代码状态：`已实现，待手测`

## 2. 测试目标

- 验证真实 HTTP LLM Provider（C++/FHttpModule）可生成合法 Plan。
- 验证配置文件缺失时可回退到关键词规划器。
- 验证 HTTP 异常时可回退到关键词规划器且错误码收敛。

## 3. 前置条件

- 配置文件：`Saved/HCIAbilityKit/Config/llm_provider.local.json`
- 默认 endpoint：`https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions`
- 默认 model：`qwen3.5-plus`

## 4. UE 手测命令

1. `HCI.AgentPlanWithRealLLMDemo "整理临时目录资产并归档"`
2. `HCI.AgentPlanWithRealLLMDemo "扫描关卡里缺碰撞和默认材质"`
3. `HCI.AgentPlanWithRealLLMDemo "把高面数网格做LOD检查"`
4. `HCI.AgentPlanWithRealLLMProbe "你是谁"`（原始响应探针）
5. `HCI.AgentPlanWithRealLLMDemo config_missing "整理临时目录资产并归档"`
6. `HCI.AgentPlanWithRealLLMDemo http_fail "整理临时目录资产并归档"`
7. `HCI.AgentPlanWithLLMMetricsDump`

## 5. 预期结果

- 正常路径：
  - `provider=llm`
  - `provider_mode=real_http`
  - `plan_validation=ok`
- 配置缺失路径：
  - `provider=keyword_fallback`
  - `fallback_reason=llm_config_missing`
  - `error_code=E4307`
- HTTP 异常路径：
  - `provider=keyword_fallback`
  - `fallback_reason=llm_http_error`
  - `error_code=E4306`

## 6. 实测结果

- 待用户填写。

## 7. 证据

- 待补 UE OutputLog 关键行。

## 8. 结论

- 当前状态：`Pending UE Pass`
