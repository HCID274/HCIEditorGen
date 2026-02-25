# AI 可调用接口总表（单一入口）

> 更新时间：2026-02-25  
> 目标：作为“语义识别 -> 工具调用 -> 提示词编写”的唯一查阅文件。  
> 适用：`HCIAbilityKit` Agent Planner / Executor / Prompt 维护。

## 1. 使用规则（强制）

- 以后新增工具、删减工具、参数变化、执行接线变化，必须先更新本文件。
- 之后再更新对应 Skill：`tools_schema.json` 与 `prompt.md`，最后改代码。
- 回答“当前有哪些能力可调”时，必须按本文件的四层口径回答：
  - `可规划（Planner）`
  - `白名单（Runtime Registry）`
  - `已实接可执行（ToolAction）`
  - `未接线将模拟（Executor Fallback）`

## 2. 事实源与优先级

1. Planner 可规划工具源：`Source/HCIEditorGen/文档/提示词/Skills/H3_AgentPlanner/tools_schema.json`
2. Runtime 白名单源：`Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Agent/HCIAbilityKitToolRegistry.cpp`
3. ToolAction 实接源：`Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/AgentActions/HCIAbilityKitAgentToolActions.cpp` (`BuildStageIDraftActions`)
4. 模拟回退源：`Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Agent/HCIAbilityKitAgentExecutor.cpp` (`HCI_TryRunToolAction` fallback 分支)

## 3. Plan JSON 调用契约（给 LLM 的输出目标）

- 顶层：
  - `intent: string`
  - `route_reason: string`
  - `steps: Step[]`
- `Step`：
  - `step_id: string`
  - `tool_name: enum`
  - `args: object`（必须匹配对应工具参数）
  - `expected_evidence: string[]`
- 管道变量：
  - 支持 `{{step_id.evidence_key}}`
  - 支持 `{{step_id.evidence_key[index]}}`

## 4. 工具接口总表（当前 8 个）

### 4.1 ScanAssets

- `tool_name`: `ScanAssets`
- `capability`: `ReadOnly`
- `args`:
  - `directory?: string`（建议 `/Game/...`）
- `执行接线状态`: `已实接可执行`
- `DryRun/Execute`: 均为真实扫描（`UEditorAssetLibrary::ListAssets`）
- `关键 evidence`:
  - `scan_root`
  - `asset_count`
  - `asset_paths`

### 4.2 SearchPath

- `tool_name`: `SearchPath`
- `capability`: `ReadOnly`
- `args`:
  - `keyword: string`（1~64）
- `执行接线状态`: `已实接可执行`
- `DryRun/Execute`: 均为真实目录检索（AssetRegistry 全路径枚举 + 模糊匹配 + 语义回退）
- `关键 evidence`:
  - `keyword`
  - `keyword_normalized`
  - `keyword_expanded`
  - `matched_directories`
  - `best_directory`
  - `semantic_fallback_used`
  - `semantic_fallback_directory`

### 4.3 SetTextureMaxSize

- `tool_name`: `SetTextureMaxSize`
- `capability`: `Write`
- `args`:
  - `asset_paths: string[]`（1~50）
  - `max_size: int`（`256|512|1024|2048|4096|8192`）
- `执行接线状态`: `未实接（当前走 Executor simulated）`
- `说明`: 允许规划与校验；在 Stage I 当前接线里未绑定 ToolAction 实现。

### 4.4 SetMeshLODGroup

- `tool_name`: `SetMeshLODGroup`
- `capability`: `Write`
- `args`:
  - `asset_paths: string[]`（1~50）
  - `lod_group: string`（`LevelArchitecture|SmallProp|LargeProp|Foliage|Character`）
- `执行接线状态`: `未实接（当前走 Executor simulated）`
- `说明`: 允许规划与校验；LOD 安全门禁仍在 Executor Preflight 生效。

### 4.5 ScanLevelMeshRisks

- `tool_name`: `ScanLevelMeshRisks`
- `capability`: `ReadOnly`
- `args`:
  - `scope: string`（`selected|all`）
  - `checks: string[]`（`missing_collision|default_material` 子集，1~2）
  - `max_actor_count: int`（1~5000）
- `执行接线状态`: `未实接（当前走 Executor simulated）`
- `说明`: 允许规划与校验；当前 Stage I ToolAction 未绑定真实执行器。

### 4.6 NormalizeAssetNamingByMetadata

- `tool_name`: `NormalizeAssetNamingByMetadata`
- `capability`: `Write`
- `args`:
  - `asset_paths: string[]`（1~50）
  - `metadata_source: string`（`auto|UAssetImportData|AssetUserData`）
  - `prefix_mode: string`（`auto_by_asset_class`）
  - `target_root: string`（`/Game/...`）
- `执行接线状态`: `未实接（当前走 Executor simulated）`
- `说明`: 允许规划与校验；当前 Stage I ToolAction 未绑定真实执行器。

### 4.7 RenameAsset

- `tool_name`: `RenameAsset`
- `capability`: `Write`
- `args`:
  - `asset_path: string`（`/Game/...`）
  - `new_name: string`（1~64，正则 `^[A-Za-z0-9_]+$`）
- `执行接线状态`: `已实接可执行`
- `DryRun`: 产出 `before/after` 变更预览
- `Execute`: 真实改名（`UEditorAssetLibrary::RenameAsset`）+ Redirector 修复（`FixupReferencers`）
- `关键 evidence`:
  - `before`
  - `after`
  - `redirector_fixup`

### 4.8 MoveAsset

- `tool_name`: `MoveAsset`
- `capability`: `Write`
- `args`:
  - `asset_path: string`（`/Game/...`）
  - `target_path: string`（`/Game/...`，可目录或对象路径）
- `执行接线状态`: `已实接可执行`
- `DryRun`: 产出 `before/after` 变更预览
- `Execute`: 真实移动（AssetTools RenameAssets 路径迁移）+ Redirector 修复（`FixupReferencers`）
- `关键 evidence`:
  - `before`
  - `after`
  - `redirector_fixup`

## 5. 当前接线结论（给提示词维护者）

- 当前可规划：8/8（由 `tools_schema.json` 与 ToolRegistry 支持）
- 当前已实接可执行：4/8（`ScanAssets/SearchPath/RenameAsset/MoveAsset`）
- 当前未实接会模拟：4/8（`SetTextureMaxSize/SetMeshLODGroup/ScanLevelMeshRisks/NormalizeAssetNamingByMetadata`）
- 因此提示词写法要求：
  - 可以规划全部 8 个工具；
  - 但在“真实执行承诺”描述中，必须按实接状态区分，不得把 8 个都表述为已真实执行。

## 6. UE 入口接口（人工触发）

- `HCIAbilityKit.AgentPlanPreviewUI "<自然语言>"`
  - 作用：触发真实 LLM 规划并弹出 Plan Preview UI。
- `HCIAbilityKit.AgentPlanWithRealLLMDemo "<自然语言>"`
  - 作用：真实 LLM 规划（日志链路）。
- `HCIAbilityKit.AgentPlanWithRealLLMProbe "<文本>"`
  - 作用：探测 LLM HTTP 可用性与原始响应。

## 7. 维护检查清单（每次改动后）

1. 本文件已更新（工具/参数/接线状态一致）。
2. `H3_AgentPlanner/tools_schema.json` 已同步（若有参数边界变化）。
3. `H3_AgentPlanner/prompt.md` 已同步（若有路由/约束变化）。
4. Runtime/Editor 代码已同步并通过编译或自动化验证。
