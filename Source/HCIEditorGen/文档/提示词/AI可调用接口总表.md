# AI 可调用接口总表（单一入口）

> 更新时间：2026-02-27  
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
2. Runtime 白名单源：`Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Agent/Tools/HCIAbilityKitToolRegistry.cpp`
3. ToolAction 实接源：`Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/AgentActions/HCIAbilityKitAgentToolActions.cpp` (`BuildStageIDraftActions`)
4. 模拟回退源：`Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Agent/Executor/HCIAbilityKitAgentExecutor.cpp` (`HCI_TryRunToolAction` fallback 分支)

## 3. Plan JSON 调用契约（给 LLM 的输出目标）

- 顶层：
  - `intent: string`
  - `route_reason: string`
  - `assistant_message?: string`（可选；用于无需工具时直接回复用户）
  - `steps: Step[]`
- 语义规则（冻结）：
  - 若 `steps=[]` 且 `assistant_message` 非空：视为“语义直接回复”（不进入执行链路）。
  - 若同时存在 `steps` 与 `assistant_message`：按“行动优先”，优先执行 `steps`；`assistant_message`仅作前导说明。
- `Step`：
  - `step_id: string`
  - `tool_name: enum`
  - `args: object`（必须匹配对应工具参数）
  - `expected_evidence: string[]`
  - `ui_presentation?: object`（可选，人话展示增强；缺失时由 C++ 自动降级）
    - `step_summary?: string`（步骤摘要，给美术看的短句）
    - `intent_reason?: string`（为什么要做这一步）
    - `risk_warning?: string`（仅写/高风险步骤建议填写）
- 管道变量：
  - 支持 `{{step_id.evidence_key}}`
  - 支持 `{{step_id.evidence_key[index]}}`

## 4. 工具接口总表（当前 9 个）

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

### 4.2 ScanMeshTriangleCount

- `tool_name`: `ScanMeshTriangleCount`
- `capability`: `ReadOnly`
- `args`:
  - `directory?: string`（建议 `/Game/...`）
- `执行接线状态`: `已实接可执行`
- `DryRun/Execute`: 均为真实扫描（遍历目录并统计 `StaticMesh` LOD0 三角面数）。
- `关键 evidence`:
  - `scan_root`
  - `scanned_count`
  - `mesh_count`
  - `max_triangle_count`
  - `max_triangle_asset`
  - `top_meshes`
  - `result`

### 4.3 SearchPath

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

### 4.4 SetTextureMaxSize

- `tool_name`: `SetTextureMaxSize`
- `capability`: `Write`
- `args`:
  - `asset_paths: string[]`（1~50，`requires_pipeline_input=true`，必须来自 `{{step_x.asset_paths}}`）
  - `max_size: int`（`256|512|1024|2048|4096|8192`）
- `PlanReady 结构门禁`:
  - 修改意图（`batch_fix_asset_compliance`）下，计划中必须存在至少一个写步骤。
  - `asset_paths` 必须消费前置 `ScanAssets` 的 `asset_paths` 证据（禁止直接写目录或硬编码路径）。
- `执行接线状态`: `已实接可执行`
- `DryRun/Execute`: 已接线真实实现（Texture2D 校验、`MaxTextureSize` 写入与资产保存）。
- `关键 evidence`:
  - `target_max_size`
  - `scanned_count`
  - `modified_count`
  - `failed_count`
  - `modified_assets`
  - `failed_assets`
  - `result`

### 4.5 SetMeshLODGroup

- `tool_name`: `SetMeshLODGroup`
- `capability`: `Write`
- `args`:
  - `asset_paths: string[]`（1~50，`requires_pipeline_input=true`，必须来自 `{{step_x.asset_paths}}`）
  - `lod_group: string`（`LevelArchitecture|SmallProp|LargeProp|Foliage|Character`）
- `PlanReady 结构门禁`:
  - `asset_paths` 必须消费前置 `ScanAssets` 的 `asset_paths` 证据（禁止绕过管道变量）。
- `执行接线状态`: `已实接可执行`
- `DryRun/Execute`: 已接线真实实现（StaticMesh 校验、`LODGroup` 写入与资产保存）。
- `关键 evidence`:
  - `target_lod_group`
  - `scanned_count`
  - `modified_count`
  - `failed_count`
  - `modified_assets`
  - `failed_assets`
  - `result`

### 4.6 ScanLevelMeshRisks

- `tool_name`: `ScanLevelMeshRisks`
- `capability`: `ReadOnly`
- `args`:
  - `scope: string`（`selected|all`，必填；默认策略为 `selected`）
  - `checks: string[]`（`missing_collision|default_material` 子集，1~2）
  - `max_actor_count: int`（1~5000）
  - `actor_names?: string[]`（1~50，可选；用于精准指定目标 Actor）
- `执行接线状态`: `已实接可执行`
- `DryRun/Execute`: 均为真实关卡扫描（Actor 遍历 + 碰撞/默认材质风险检测）。
- `selected-first 语义`：
  - 若 `scope=selected` 且无选中目标，返回 `ErrorCode=no_actors_selected`（不允许隐式升级为全量扫描）。
  - 仅当用户显式表达“全场景扫描”时才允许 `scope=all`。
- `关键 evidence`:
  - `actor_path`
  - `issue`
  - `evidence`
  - `scope`
  - `scanned_count`
  - `risky_count`
  - `risk_summary`
  - `risky_actors`
  - `missing_collision_actors`
  - `default_material_actors`
  - `result`

### 4.7 NormalizeAssetNamingByMetadata

- `tool_name`: `NormalizeAssetNamingByMetadata`
- `capability`: `Write`
- `args`:
  - `asset_paths: string[]`（1~50）
  - `metadata_source: string`（`auto|UAssetImportData|AssetUserData`）
  - `prefix_mode: string`（`auto_by_asset_class`）
  - `target_root: string`（`/Game/...`）
- `执行接线状态`: `已实接可执行`
- `DryRun/Execute`: 已接线真实实现（按资产类型前缀生成规范命名，输出提案并执行真实改名/移动到 `target_root`）。
- `关键 evidence`:
  - `result`
  - `proposed_renames`
  - `proposed_moves`
  - `affected_count`

### 4.8 RenameAsset

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

### 4.9 MoveAsset

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

### 4.10 AutoMaterialSetupByNameContract

- `tool_name`: `AutoMaterialSetupByNameContract`
- `capability`: `Write`
- `args`:
  - `asset_paths: string[]`（1~50，必须来自 `ScanAssets.asset_paths` 管道变量）
  - `target_root: string`（`/Game/...`；将创建 `Materials/MI_<ID>`）
- `master_material_path?: string`（可选；缺省时读取 `Saved/HCIAbilityKit/TestFixtures/MatLink/latest.json` 的 `snapshot_master_material_object_path`）
- `执行接线状态`: `已实接可执行（待 Stage O-SliceO1 UE 手测门禁）`
- `DryRun/Execute`:
  - `DryRun`：只产出“将创建的 MI + 将绑定的参数 + 将修改的 Mesh Slot0”清单，不创建资产
  - `Execute`：创建 `MaterialInstanceConstant` 并绑定贴图参数，默认挂载到 Mesh 的 Slot0（必须 `requires_confirm=true`）
- `关键 evidence`:
  - `group_count`
  - `proposed_material_instances`
  - `proposed_mesh_assignments`
  - `proposed_parameter_bindings`
  - `missing_groups`
  - `result`

## 5. 当前接线结论（给提示词维护者）

- 当前可规划：10/10（由 `tools_schema.json` 与 ToolRegistry 支持）
- 当前已实接可执行：10/10
- 当前未实接会模拟：0/10
- 因此提示词写法要求：
  - 可以规划全部 10 个工具；
  - 10 个工具均已具备真实执行接线，仍需遵循既有确认门禁与风险分级。

## 6. UE 入口接口（人工触发）

- `HCIAbilityKit.AgentChatUI [optional initial text]`
  - 作用：打开聊天式自然语言入口窗口（Stage I6/I8）；默认只渲染计划卡片，由用户点击按钮后才打开 `PlanPreview`。
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
