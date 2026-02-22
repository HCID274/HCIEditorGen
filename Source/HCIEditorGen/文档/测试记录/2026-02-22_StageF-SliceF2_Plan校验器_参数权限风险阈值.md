# StageF-SliceF2 Plan 校验器（参数完整性 / 权限 / 风险 / 阈值）（2026-02-22）

- 日期：2026-02-22
- 切片编号：StageF-SliceF2
- 测试人：助手（实现/编译/自动化）+ 用户（UE 手测）
- 当前状态：已完成（用户 UE 手测通过）

## 1. 测试目标

- 落地 `Plan` 校验器（基于 `ToolRegistry args_schema`）并冻结错误码口径：
  - `E4001`：缺失必填字段
  - `E4002`：工具不在白名单
  - `E4004`：累计修改目标数超阈值（`MAX_ASSET_MODIFY_LIMIT=50`）
  - `E4009`：参数枚举/边界/未声明字段非法
  - `E4011`：`ScanLevelMeshRisks` 的 `scope/checks` 参数非法
  - `E4012`：`NormalizeAssetNamingByMetadata` 元数据不足且无法生成安全命名提案（F2 mock seam）
- 校验 `risk_level/requires_confirm` 与工具能力的一致性（不一致返回 `E4003`）。
- 提供 UE 控制台命令 `HCIAbilityKit.AgentPlanValidateDemo` 用于手测固定案例与自定义单案例。

## 2. 影响范围

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Public/Agent/HCIAbilityKitAgentPlanValidator.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Agent/HCIAbilityKitAgentPlanValidator.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/HCIAbilityKitEditorModule.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Tests/HCIAbilityKitAgentPlanValidationTests.cpp`
- `Source/HCIEditorGen/文档/00_总进度.md`
- `Source/HCIEditorGen/文档/03_Schema_v1与错误契约.md`
- `Source/HCIEditorGen/文档/04_代码框架总览_树状图.md`
- `AGENTS.md`

## 3. 前置条件

- `StageF-SliceF1` 已通过。
- 工程可编译。
- UE 编辑器可打开并执行控制台命令。

## 4. 操作步骤

1. 编译（助手已完成）
   - `Build.bat HCIEditorGenEditor Win64 Development -Project=... -WaitMutex -FromMSBuild`
2. 自动化测试（助手已完成）
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AgentPlanValidation; Quit"`
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AgentPlan; Quit"`（F1 回归）
   - `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.AgentTools; Quit"`（ToolRegistry 回归）
3. UE 手测（默认 7 案例摘要）
   - `HCIAbilityKit.AgentPlanValidateDemo`
4. UE 手测（单案例：工具白名单拦截）
   - `HCIAbilityKit.AgentPlanValidateDemo fail_unknown_tool`
5. UE 手测（单案例：关卡排雷非法 scope）
   - `HCIAbilityKit.AgentPlanValidateDemo fail_level_scope`
6. UE 手测（单案例：命名元数据不足 mock）
   - `HCIAbilityKit.AgentPlanValidateDemo fail_naming_metadata`

## 5. 预期结果（Pass 判定标准）

1. 上述 4 条命令（步骤 3~6）在合法参数下均无 `Error` 级日志。
2. `HCIAbilityKit.AgentPlanValidateDemo`（无参）输出 `7` 条案例日志 + `1` 条摘要，且摘要包含：
   - `summary total_cases=7`
   - `supports_checks=schema|whitelist|risk|threshold|special_cases`
   - `validation=ok`
3. 无参案例集合中应至少覆盖以下错误码与成功案例（可通过 `case=` 行核对）：
   - `ok_naming`（`valid=true`）
   - `fail_missing_tool`（`error_code=E4001`）
   - `fail_unknown_tool`（`error_code=E4002`）
   - `fail_invalid_enum`（`error_code=E4009`）
   - `fail_over_limit`（`error_code=E4004`）
   - `fail_level_scope`（`error_code=E4011`）
   - `fail_naming_metadata`（`error_code=E4012`）
4. `HCIAbilityKit.AgentPlanValidateDemo fail_unknown_tool` 输出命中：
   - `valid=false`
   - `error_code=E4002`
   - `reason=tool_not_whitelisted`
5. `HCIAbilityKit.AgentPlanValidateDemo fail_level_scope` 输出命中：
   - `valid=false`
   - `error_code=E4011`
   - `field=steps[0].args.scope`
6. `HCIAbilityKit.AgentPlanValidateDemo fail_naming_metadata` 输出命中：
   - `valid=false`
   - `error_code=E4012`
   - `reason=naming_metadata_insufficient_no_safe_proposal`

## 6. 实际结果（助手本地验证）

- 编译：通过
  - `Build.bat HCIEditorGenEditor Win64 Development ...` 成功。
- 自动化：通过
  - `HCIAbilityKit.Editor.AgentPlanValidation`：7/7 成功
    - `AcceptsValidPlan`
    - `MissingRequiredFieldReturnsE4001`
    - `UnknownToolReturnsE4002`
    - `InvalidEnumReturnsE4009`
    - `OverModifyLimitReturnsE4004`
    - `LevelRiskInvalidScopeReturnsE4011`
    - `NamingMetadataInsufficientReturnsE4012`
  - `HCIAbilityKit.Editor.AgentPlan`：3/3 成功（回归）
  - `HCIAbilityKit.Editor.AgentTools`：3/3 成功（回归）
- 说明：
  - `UnrealEditor-Cmd` 在当前环境常无控制台回显，以 `Saved/Logs/HCIEditorGen.log` 中 `Result={成功}` 为准。
  - 并行启动多个 `UnrealEditor-Cmd` 会争抢日志文件；本次最终证据以串行回归结果为准。
- UE 手测：通过。
  - 4 条命令（无参、`fail_unknown_tool`、`fail_level_scope`、`fail_naming_metadata`）均无 `Error`。
  - 无参摘要命中：
    - `summary total_cases=7`
    - `supports_checks=schema|whitelist|risk|threshold|special_cases`
    - `validation=ok`
  - 无参案例覆盖并命中：
    - `ok_naming -> valid=true`
    - `fail_missing_tool -> E4001`
    - `fail_unknown_tool -> E4002`
    - `fail_invalid_enum -> E4009`
    - `fail_over_limit -> E4004`
    - `fail_level_scope -> E4011`
    - `fail_naming_metadata -> E4012`
  - 单案例命中：
    - `fail_unknown_tool -> valid=false error_code=E4002 reason=tool_not_whitelisted`
    - `fail_level_scope -> valid=false error_code=E4011 field=steps[0].args.scope`
    - `fail_naming_metadata -> valid=false error_code=E4012 reason=naming_metadata_insufficient_no_safe_proposal`

## 7. 结论

- `Pass`

## 8. 证据

- 构建日志：`C:\Users\50428\AppData\Local\UnrealBuildTool\Log.txt`
- 自动化主日志：`Saved/Logs/HCIEditorGen.log`
- 自动化关键证据（助手本地已核对）：
  - `Result={成功} Name={AcceptsValidPlan}`
  - `Result={成功} Name={MissingRequiredFieldReturnsE4001}`
  - `Result={成功} Name={UnknownToolReturnsE4002}`
  - `Result={成功} Name={InvalidEnumReturnsE4009}`
  - `Result={成功} Name={OverModifyLimitReturnsE4004}`
  - `Result={成功} Name={LevelRiskInvalidScopeReturnsE4011}`
  - `Result={成功} Name={NamingMetadataInsufficientReturnsE4012}`

## 9. 问题与后续动作

- `StageF-SliceF2` 门禁关闭。
- 主线切换到 `StageF-SliceF3`（计划执行骨架与收敛日志）。
