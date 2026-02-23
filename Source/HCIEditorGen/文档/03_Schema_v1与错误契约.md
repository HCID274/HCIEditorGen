# HCIAbilityKit Schema v1 与错误契约

> 状态：冻结（M0-V3）
> 版本：v3
> 更新时间：2026-02-21

## 1. 数据输入契约（SourceData）

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
    "damage": 120.0
  }
}
```

- `schema_version`：固定 `1`
- `id`：非空、全局唯一，建议 `[a-z0-9_]+`
- `display_name`：字符串，可空但建议可读
- `params.damage`：数值，建议范围 `0.0 ~ 100000.0`

### 1.1 检索压测扩展字段（可选，向后兼容）

- 目的：支持 Step3 检索压测与语义能力对比，不改变 Runtime 解析必填项。
- 允许新增字段：
  - `name`：展示名（可与 `display_name` 同步）
  - `type`：`StaticMesh/Sound/VFX`
  - `tags`：字符串数组（语义标签、陷阱标记等）
  - `description`：长文本描述（用于语义理解与复合查询）
  - `params`：可扩展更多参数（如 `size/radius/duration/cooldown/...`），但 `params.damage` 仍为必填
- 兼容性约束：
  - 导入链路继续按 `schema_version/id/display_name/params.damage` 验证；
  - 额外字段不得破坏现有导入与重导入行为。

### 1.2 资产审计扩展字段（可选，向后兼容）

- 目的：支撑资产性能审计与规则诊断，不改变现有导入必填项。
- 建议字段：
  - `virtual_path`：用于“目录语义 vs 实际复杂度”审计。
  - `representing_mesh`：代表性网格路径（示例：`/Game/Seed/SM_HighPoly_01.SM_HighPoly_01`）。
  - `params.triangle_count_lod0`：用于性能阈值审计（可作为离线样本字段）。
  - `tags`：可附加 `trap:performance` / `expected:lowpoly` 等诊断标签。
- 约束：
  - `representing_mesh` 必须使用 UE 长对象路径格式：`/Game/.../AssetName.AssetName`（禁止 OS 文件路径如 `D:\...\*.uasset`）。
  - 新字段缺失时，审计链路必须可降级运行；
  - 新字段存在时，不应影响 Import/Reimport 成功路径。
  - `representing_mesh` 存在但路径无效时，应给出可定位错误，不得静默吞掉。
  - Python 生成脚本仅负责写入路径字符串；路径解析与有效性校验统一由 `UHCIAbilityKitFactory` 执行并触发 `E1006`。

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

## 5. 错误码（v3）

- `E1001`：JSON 格式非法
- `E1002`：缺失必填字段
- `E1003`：字段类型错误
- `E1004`：字段值越界或非法
- `E1005`：源文件不存在或不可读
- `E1006`：`representing_mesh` 路径非法或目标资产不存在
- `E2001`：Skill 未命中或命中冲突
- `E2002`：敏感操作未获授权
- `E3001`：审计未通过（高风险）
- `E4001`：Plan JSON 缺失必填字段
- `E4002`：Plan JSON 工具不在白名单
- `E4003`：Tool 参数校验失败
- `E4004`：超出批量修改阈值
- `E4005`：用户未确认执行写操作
- `E4006`：SourceControl Checkout 失败（Fail-Fast）
- `E4007`：事务执行失败并已整单回滚
- `E4008`：本地权限校验失败（Mock RBAC）
- `E4009`：工具参数超出边界或枚举非法
- `E4010`：LOD 工具目标资产类型不支持或 Nanite 已启用
- `E4011`：关卡风险扫描目标非法（非 Actor 范围或参数不合法）
- `E4012`：命名溯源信息缺失且无法生成安全命名提案

## 6. 测试样例（固定）

- `TC_OK_01`：合法导入与重导
- `TC_FAIL_01`：删除 `id`（期望 `E1002`）
- `TC_FAIL_02`：`damage: "abc"`（期望 `E1003`）
- `TC_FAIL_03`：源文件失效（期望 `E1005`）
- `TC_FAIL_04`：敏感操作拒绝授权（期望 `E2002`）
- `TC_FAIL_05`：`representing_mesh` 路径不存在（期望 `E1006`）
- `TC_AGENT_FAIL_01`：Plan 缺少 `tool_name`（期望 `E4001`）
- `TC_AGENT_FAIL_02`：Tool 不在白名单（期望 `E4002`）
- `TC_AGENT_FAIL_03`：变更资产数 `>50`（期望 `E4004`）
- `TC_AGENT_FAIL_04`：未确认执行写操作（期望 `E4005`）
- `TC_AGENT_FAIL_05`：`Checkout` 失败（期望 `E4006`）
- `TC_AGENT_FAIL_06`：步骤失败触发全单回滚（期望 `E4007`）
- `TC_AGENT_OK_02`：SourceControl 未启用（期望 Warning + 离线本地模式放行）
- `TC_AGENT_FAIL_07`：未命中用户名执行写操作（期望 `E4008`，Guest 只读）
- `TC_AGENT_FAIL_08`：`SetMeshLODGroup` 作用于 Nanite 或非 StaticMesh（期望 `E4010`）
- `TC_AGENT_OK_03`：`ScanLevelMeshRisks` 扫描选中/全量 Actor，输出 `missing_collision/default_material` 证据并可定位
- `TC_AGENT_FAIL_09`：`ScanLevelMeshRisks` 传入非法 `scope/checks`（期望 `E4011`）
- `TC_AGENT_OK_04`：`NormalizeAssetNamingByMetadata` 可读取 `UAssetImportData/AssetUserData` 生成前缀命名与 Move 计划（Dry-Run）
- `TC_AGENT_FAIL_10`：命名归档工具元数据不足且无法安全推断（期望 `E4012`）

## 7. 检索文档契约（Step3-Slice1，历史基线）

- 目标结构：`FHCIAbilitySearchDocument`（Runtime，纯只读检索语义）。
- 说明：该节用于保留旧检索主线历史契约，当前资产审计主线不以此为门禁。
- 字段最小集（冻结）：
  - `AssetPath`：资产路径（审计与定位）。
  - `Id` / `DisplayName` / `Damage`：基础检索字段。
  - `Element`：`Unknown/Fire/Ice/Nature/Lightning/Physical`。
  - `DamageTier`：`Low/Medium/High`（当前阈值：`<=120`、`<=300`、`>300`）。
  - `ControlProfile`：`None/SoftControl/HardControl`。
  - `UsageScenes`：`General/Forest/BossPhase2`。
  - `Tokens`：基础词 + 语义扩展词（中英混合）。
- 索引结构（冻结）：
  - `DocumentsById`
  - `IdsByElement`
  - `IdsByDamageTier`
  - `IdsByControlProfile`
  - `IdsByUsageScene`
  - `IdsByToken`
- 约束：
  - 仅定义结构和语义映射，不在本切片实现资产扫描与增量刷新。
  - 该结构需覆盖场景 `S1/S2/S3` 的过滤与解释字段需求。

## 8. 演进规则

- 仅允许向后兼容升级（新增字段必须有默认值）。
- `schema_version` 升级时，必须提供迁移策略。
- 变更契约前，先更新测试样例和门禁文档。

## 9. 资产面数提取契约（资产审计主线）

- 目标：明确 `Triangle Count` 获取路径，避免扫描阶段误判。
- 提取优先级（固定）：
  1. `tag_cached`：优先读取 `RepresentingMesh` 对应 `AssetRegistry Tags`（示例键：`hci_triangles_lod0`）。
  2. `mesh_loaded`：Tag 缺失或网格信号不完整时，在 Stage D 分批加载 `RepresentingMesh` 对应 `UStaticMesh` 并提取 `LOD0` 三角面数（`GetNumTriangles(0)`）与网格信号（`LOD/Nanite`）。
  3. `unavailable`：无法提取时标记未知，不得伪造数值。
- 口径优先级（固定）：
  1. `triangle_count_lod0_actual`（来自真实 `RepresentingMesh`）为最终审计依据；
  2. `params.triangle_count_lod0` 仅作为 `expected` 预期值；
  3. 当 `actual != expected` 时，触发 `TriangleExpectedMismatchRule`（`severity=Warn`，即 MismatchWarn）。
- 审计证据最小字段：
  - `representing_mesh_path`
  - `triangle_count_lod0_actual`
  - `triangle_count_lod0_expected_json`（可缺省）
  - `triangle_mismatch_delta`（可缺省）
  - `triangle_source`（`tag_cached/mesh_loaded/unavailable`）
  - `lod_index`（默认 `0`）
  - `source_tag_key`（当来源为 Tag 时必填）
- 资产标签落地（Stage C1 实装）：
  - 导入时若 JSON 存在 `params.triangle_count_lod0`，写入 AssetRegistry Tag：`hci_triangle_expected_lod0`；
  - 扫描时读取该 Tag 作为 `triangle_count_lod0_expected_json` 的默认来源。
- Stage C2 规则读取的 UE 原生标签（已实现）：
  - `StaticMesh`：`Triangles`、`LODs`、`NaniteEnabled`
  - `Texture2D`：`Dimensions`（格式 `WxH`）
- 性能约束：
  - Stage B 元数据扫描禁止默认 `LoadObject`，仅验证 `representing_mesh_path` 可解析与可索引；
  - Stage D 深度提取必须批次化并记录峰值内存。
  - 遇到 `Locked` 或 `Dirty 且无法安全读取` 资产时必须跳过，并在证据中标记 `scan_state=skipped_locked_or_dirty`。

## 9.1 Runtime 资产对象扩展契约（Stage B 启动前冻结）

```cpp
UPROPERTY(EditAnywhere, Category="Audit")
TSoftObjectPtr<UStaticMesh> RepresentingMesh;
```

- 语义：`RepresentingMesh` 仅用于“代表性几何体”审计与场景可视化，不参与技能数值逻辑。
- 导入策略：JSON 存在 `representing_mesh` 时优先按路径绑定；缺失时可由导入脚本按种子清单回填。
- 审计策略：规则与报告必须优先使用该字段追踪真实网格证据。

## 10. 审计规则注册契约（Rule Registry）

- Runtime 规则接口（冻结建议）：
```cpp
class IHCIAbilityAuditRule
{
public:
    virtual ~IHCIAbilityAuditRule() = default;
    virtual FName GetRuleId() const = 0;
    virtual void Evaluate(const FHCIAbilityAuditContext& Context, FHCIAbilityAuditResult& OutResult) const = 0;
};
```
- 注册中心职责：
  - 统一管理规则实例生命周期；
  - 支持按 `rule_id` 启停；
  - 支持新增规则不改扫描主流程（开闭原则）。
- 基础规则首批集合：
  - `NamingRule`
  - `PathConventionRule`
  - `TriangleBudgetRule`
  - `TextureNPOTRule`
  - `HighPolyAutoLODRule`
  - `MissingCollisionRule`
  - `DefaultMaterialRule`
  - `MetadataNamingRule`
  - `ArchiveMoveRule`
  - `TriangleExpectedMismatchRule`（actual vs expected）
  - `PathComplexityMismatchRule`

## 11. 审计结果 JSON 契约（Stage C 主线）

- 输出目标：统一承载资产审计结果，支持导出与追踪。
- 严重级别枚举：
  - `Info`
  - `Warn`
  - `Error`
- 最小结构：
```json
{
  "run_id": "audit_20260215_001",
  "generated_utc": "2026-02-15T09:30:00Z",
  "results": [
    {
      "asset_path": "/Game/Art/Env/LowPoly/Rocks/SM_small_rock_01",
      "rule_id": "TriangleBudgetRule",
      "severity": "Error",
      "reason": "triangle count exceeds budget for lowpoly asset",
      "hint": "reduce LOD0 triangles or move to highpoly path",
      "evidence": {
        "representing_mesh_path": "/Game/Seed/SM_AI_Mesh_01.SM_AI_Mesh_01",
        "triangle_count_lod0_actual": 100000,
        "triangle_count_lod0_expected_json": 500,
        "triangle_mismatch_delta": 99500,
        "triangle_budget": 500,
        "triangle_source": "tag_cached",
        "lod_index": 0,
        "source_tag_key": "hci_triangles_lod0",
        "virtual_path": "/Game/Art/Env/LowPoly/Rocks/",
        "scan_state": "ok"
      }
    }
  ]
}
```
- `evidence` 最小字段：
  - `representing_mesh_path`（可缺省）
  - `triangle_count_lod0_actual`（可缺省）
  - `triangle_count_lod0_expected_json`（可缺省）
  - `triangle_source`（`tag_cached/mesh_loaded/unavailable`）
  - `actor_path`（关卡风险规则可填）
  - `missing_collision`（`true/false`，关卡风险规则可填）
  - `uses_default_material`（`true/false`，关卡风险规则可填）
  - `metadata_source`（`UAssetImportData/AssetUserData/auto`，命名溯源规则可填）
  - `importer`（可缺省）
  - `import_timestamp`（可缺省）
  - `proposed_name`（可缺省）
  - `proposed_target_path`（可缺省）
  - `virtual_path`（可缺省）
  - `scan_state`（`ok/skipped_locked_or_dirty/skipped_unavailable`）
- StageC-SliceC3 实现口径（2026-02-22）：
  - Runtime 新增统一结果结构：`FHCIAbilityKitAuditReport` / `FHCIAbilityKitAuditResultEntry`；
  - 结果构建器：`FHCIAbilityKitAuditReportBuilder::BuildFromSnapshot(...)`，将 `ScanSnapshot.Rows[*].AuditIssues[*]` 扁平化为 `results[]`；
  - 严重级别统一字符串化：`Info/Warn/Error`（`SeverityToString`），供日志展示与下一切片 JSON 导出复用；
  - 构建器保证核心追踪证据兜底：`scan_state`，以及可用时补齐 `triangle_source/representing_mesh_path`。
- StageC-SliceC4 实现口径（2026-02-22）：
  - Runtime 新增 `FHCIAbilityKitAuditReportJsonSerializer::SerializeToJsonString(...)`；
  - Editor 新增控制台命令：`HCIAbilityKit.AuditExportJson [output_json_path]`（同步扫描后直接导出）；
  - 默认导出目录：`Saved/HCIAbilityKit/AuditReports/`；
  - 输出结果项包含顶层 `triangle_source` 与 `evidence` 对象（保留规则证据键值对）；
  - `ExecCmds` 场景兼容：导出命令会清洗路径参数尾部分号 `;`，避免生成错误文件名。
- StageD-SliceD1 实现口径（2026-02-22，异步深度检查）：
  - `HCIAbilityKit.AuditScanAsync` 新增可选参数：`[deep_mesh_check=0|1]`（默认 `0`）；
  - 当 `deep_mesh_check=1` 时，仅对需要补齐网格信号的行分批同步加载 `RepresentingMesh`；
  - 提取后立即释放 `FStreamableHandle` 强引用（GC 节流由 `StageD-SliceD2` 处理）；
  - 完成日志新增深度统计行：`[HCIAbilityKit][AuditScanAsync][Deep] load_attempts/load_success/handle_releases/...`。
- StageD-SliceD2 实现口径（2026-02-22，GC 节流）：
  - `HCIAbilityKit.AuditScanAsync` 新增第 4 参数：`[gc_every_n_batches>=0]`；
  - 当开启深度模式且未显式传入第 4 参数时，默认 `gc_every_n_batches=16`；
  - 每处理 `N` 个批次后执行一次 `CollectGarbage`（`N=0` 表示关闭 GC 节流）；
  - 深度统计新增字段：`batches/gc_every_n_batches/gc_runs/max_batch_ms/max_gc_ms/peak_used_physical_mib`；
  - `Stop/Retry` 必须保留 `gc_every_n_batches` 配置（通过控制器重试上下文持久化）。
- StageD-SliceD3 实现口径（2026-02-22，峰值内存与吞吐监控）：
  - 异步扫描在进度日志点额外输出运行中性能监控行：`[HCIAbilityKit][AuditScanAsync][Perf] ...`；
  - 运行中监控最小字段：`avg_assets_per_sec/window_assets_per_sec/eta_s/used_physical_mib/peak_used_physical_mib/gc_runs`；
  - 完成后额外输出性能汇总行：`[HCIAbilityKit][AuditScanAsync][PerfSummary] ...`；
  - 汇总最小字段：`avg_assets_per_sec/p50_batch_assets_per_sec/p95_batch_assets_per_sec/max_batch_assets_per_sec/peak_used_physical_mib`；
  - 批次吞吐统计使用 nearest-rank 分位数（P50/P95），用于面试演示口径与后续 KPI 基线采样。

## 12. 索引统计契约（Step3-Slice2，历史基线）

- 输出来源：`FHCIAbilityKitSearchIndexStats::ToSummaryString()`。
- 固定日志前缀：`[HCIAbilityKit][SearchIndex]`。
- 最小统计字段：
  - `mode`：`full_rebuild` / `incremental_refresh` / `incremental_remove`
  - `docs`：当前索引文档数
  - `display_name_coverage`
  - `scene_coverage`
  - `token_coverage`
  - `refresh_ms`
  - `updated_utc`
- 验收约束：
  - Editor 启动时必须输出一次 `full_rebuild`。
  - Import/Reimport 成功后必须输出一次 `incremental_refresh`。

## 13. 查询召回契约（Step3-Slice3，历史基线）

- 入口命令：`HCIAbilityKit.Search <query text> [topk]`。
- 输出来源：`FHCIAbilityKitSearchQueryService`。
- 解析结果最小字段：
  - `topk`
  - `damage`（`any/prefer_low/prefer_medium/prefer_high`）
  - `require_any_control`
  - `lower_than_ref`
  - 可选 `element/control/scenes/similar_to`
- 日志前缀：`[HCIAbilityKit][SearchQuery]`。
- 查询日志最小字段：
  - `raw`（原始查询）
  - `parsed`（规则解析摘要）
  - `candidates`（候选总数）
  - `topk`（输出数量）
  - `top_ids`（候选 ID + 分数）
- 空结果要求：
  - 禁止静默失败；
  - 必须输出 `suggestion=...` 的可执行建议。

## 14. Agent 执行契约（Stage E/F 冻结）

### 14.1 Plan JSON 契约（必须做）

- 最小结构：
```json
{
  "plan_version": 1,
  "request_id": "req_20260221_0001",
  "intent": "batch_fix_assets",
  "steps": [
    {
      "step_id": "s1",
      "tool_name": "SetTextureMaxSize",
      "args": {
        "asset_paths": [
          "/Game/Art/Trees/T_Tree_01_D.T_Tree_01_D"
        ],
        "max_size": 1024
      },
      "risk_level": "write",
      "requires_confirm": true,
      "rollback_strategy": "all_or_nothing",
      "expected_evidence": [
        "asset_path",
        "before",
        "after"
      ]
    }
  ]
}
```
- 必填字段：
  - 顶层：`plan_version/request_id/intent/steps`
  - 步骤：`step_id/tool_name/args/risk_level/requires_confirm/rollback_strategy`
- 约束：
  - `risk_level` 仅允许：`read_only/write/destructive`；
  - `requires_confirm=true` 的步骤，未确认前禁止执行；
  - `tool_name` 必须命中 Tool Registry 白名单。

#### 14.1.1 StageF-SliceF1 落地（最小规划器 + 契约序列化）

- Runtime 新增 `Plan JSON` 契约结构：
  - `FHCIAbilityKitAgentPlan`
  - `FHCIAbilityKitAgentPlanStep`
  - `FHCIAbilityKitAgentPlanContract::ValidateMinimalContract(...)`
- Runtime 新增 `FHCIAbilityKitAgentPlanJsonSerializer`
  - 输出字段：`plan_version/request_id/intent/steps[]`
  - `steps[]` 字段包含：`step_id/tool_name/args/risk_level/requires_confirm/rollback_strategy/expected_evidence`
- Runtime 新增最小关键词规划器 `FHCIAbilityKitAgentPlanner`
  - 输入：自然语言文本 + `request_id`
  - 输出：结构化 `Plan JSON`（仅演示规划，不执行写操作）
  - 一期最小意图覆盖（F1 门禁）：
    - 命名溯源整理：支持“整理临时目录资产（命名+归档）” -> `NormalizeAssetNamingByMetadata`
    - 关卡排雷：碰撞/默认材质检查 -> `ScanLevelMeshRisks`
    - 资产规范合规：贴图尺寸/LOD 关键词 -> `SetTextureMaxSize` / `SetMeshLODGroup`
- Editor 控制台命令（F1 手测门禁）：
  - `HCIAbilityKit.AgentPlanDemo [自然语言文本...]`
    - 无参：输出三类默认案例（命名归档/关卡排雷/资产合规）摘要与 `row=` 步骤列表
    - 有参：输出自定义输入的单案例摘要与步骤列表
  - `HCIAbilityKit.AgentPlanDemoJson [自然语言文本...]`
    - 输出序列化后的 `Plan JSON` 契约文本（用于手测字段核验）

#### 14.1.2 StageF-SliceF2 落地（Plan 校验器：参数/权限/风险/阈值）

- Runtime 新增 `FHCIAbilityKitAgentPlanValidator`
  - 输入：
    - `FHCIAbilityKitAgentPlan`
    - `FHCIAbilityKitToolRegistry`（冻结白名单与 `args_schema`）
    - `FHCIAbilityKitAgentPlanValidationContext`（F2 mock seam，用于模拟命名元数据不足 -> `E4012`）
  - 输出：`FHCIAbilityKitAgentPlanValidationResult`
    - `bValid/error_code/field/reason`
    - `validated_steps/write_steps/total_modify_targets/max_risk`
    - `failed_step_index/failed_step_id/failed_tool_name`
- F2 校验范围（冻结）：
  - 最小契约校验失败 -> `E4001`（复用 `ValidateMinimalContract`）
  - `tool_name` 未命中白名单 -> `E4002`
  - `risk_level` 与工具能力不一致、`requires_confirm` 与能力不一致 -> `E4003`
  - 参数缺失/未声明字段/类型错误/边界与枚举非法：
    - 缺失必填字段 -> `E4001`
    - 未声明字段或枚举/边界非法 -> `E4009`
    - 参数类型错误 -> `E4003`
  - `ScanLevelMeshRisks` 的 `scope/checks` 非法（类型或枚举）-> `E4011`
  - 写步骤累计修改目标数 `> MAX_ASSET_MODIFY_LIMIT(50)` -> `E4004`
    - 注：单步 `asset_paths > 50` 会先命中 `args_schema` 上限（`E4009`）；`E4004` 用“多步累计超限”门禁验证
  - `NormalizeAssetNamingByMetadata` 命名元数据不足（F2 mock seam）-> `E4012`
- Editor 控制台命令（F2 手测门禁）：
  - `HCIAbilityKit.AgentPlanValidateDemo [case_key]`
    - 无参：输出固定案例集合（成功 + `E4001/E4002/E4004/E4009/E4011/E4012`）与摘要
    - 有参：运行单案例（`ok_naming/fail_missing_tool/fail_unknown_tool/fail_invalid_enum/fail_over_limit/fail_level_scope/fail_naming_metadata`）
    - 输出字段：`valid/error_code/field/reason/validated_steps/write_steps/total_modify_targets/max_risk/expected_code/expected_match`

### 14.2 Tool Registry 能力声明契约（必须做）

- 最小结构：
```json
{
  "tool_name": "SetTextureMaxSize",
  "args_schema": {
    "asset_paths": "string[]",
    "max_size": "int"
  },
  "capability": "write",
  "supports_dry_run": true,
  "supports_undo": true,
  "destructive": false
}
```
- 必填字段：
  - `tool_name/args_schema/capability/supports_dry_run/supports_undo/destructive`
- 约束：
  - `capability` 仅允许：`read_only/write/destructive`；
  - `destructive=true` 的工具必须走二次确认；
  - 未声明能力的工具不得被 Executor 调用。
- 一期首批工具白名单（冻结）：
  - `ScanAssets`（`read_only`）
  - `SetTextureMaxSize`（`write`）
  - `SetMeshLODGroup`（`write`）
  - `ScanLevelMeshRisks`（`read_only`）
  - `NormalizeAssetNamingByMetadata`（`write`）
  - `RenameAsset`（`write`）
  - `MoveAsset`（`write`）
- 一期工具 `args_schema` 冻结（严格枚举与边界）：
  - `SetTextureMaxSize`
    - `asset_paths`: `string[]`（长度 `1..50`）
    - `max_size`: `int`（仅允许枚举：`256/512/1024/2048/4096/8192`）
  - `SetMeshLODGroup`
    - `asset_paths`: `string[]`（长度 `1..50`）
    - `lod_group`: `string`（仅允许枚举：`LevelArchitecture/SmallProp/LargeProp/Foliage/Character`）
  - `ScanLevelMeshRisks`
    - `scope`: `string`（仅允许枚举：`selected/all`）
    - `checks`: `string[]`（仅允许枚举子集：`missing_collision/default_material`）
    - `max_actor_count`: `int`（范围 `1..5000`）
  - `NormalizeAssetNamingByMetadata`
    - `asset_paths`: `string[]`（长度 `1..50`）
    - `metadata_source`: `string`（仅允许枚举：`auto/UAssetImportData/AssetUserData`）
    - `prefix_mode`: `string`（仅允许枚举：`auto_by_asset_class`，静态网格强制 `SM_`、贴图强制 `T_`）
    - `target_root`: `string`（必须以 `/Game/` 开头）
  - `RenameAsset`
    - `asset_path`: `string`
    - `new_name`: `string`（正则：`^[A-Za-z0-9_]+$`，长度 `1..64`）
  - `MoveAsset`
    - `asset_path`: `string`
    - `target_path`: `string`（必须以 `/Game/` 开头）
- 参数策略：
  - 任何不在枚举或超出边界的参数，直接返回 `E4009`；
  - `ScanLevelMeshRisks` 的 `scope/checks` 非法直接返回 `E4011`；
  - `NormalizeAssetNamingByMetadata` 无法从元数据生成安全提案时返回 `E4012`；
  - 大模型不得绕过 `args_schema` 直接注入未声明字段。

#### 14.2.1 StageE-SliceE1 落地（Runtime 冻结声明 + Editor 只读输出）

- Runtime 新增 `FHCIAbilityKitToolRegistry`（`HCIAbilityKitRuntime/Public|Private/Agent/`）：
  - 默认白名单在 Runtime 冻结，Editor/Executor 只能读取，不得隐式扩展。
  - 每个 ToolDescriptor 固定字段：
    - `tool_name`
    - `args_schema[]`
    - `capability`（`read_only/write/destructive`）
    - `supports_dry_run`
    - `supports_undo`
    - `destructive`
    - `domain`（用于覆盖三维业务：`AssetCompliance/LevelRisk/NamingTraceability`）
- `args_schema` 字段级约束结构（冻结）：
  - 值类型：`string/string[]/int`
  - 枚举：`AllowedStringValues/AllowedIntValues`
  - 边界：`Min/Max`（数组长度、字符串长度、整数范围）
  - 特殊约束：`RegexPattern`、`bMustStartWithGamePath`、`bStringArrayAllowsSubsetOfEnum`
- Editor 调试命令（只读）：
  - `HCIAbilityKit.ToolRegistryDump [tool_name]`
  - 输出目的：UE 手测核验白名单数量、能力元信息、`args_schema` 枚举与边界是否按冻结口径落地。

### 14.3 Dry-Run Diff 契约（必须做）

- 最小结构：
```json
{
  "request_id": "req_20260221_0001",
  "summary": {
    "total_candidates": 12,
    "modifiable": 10,
    "skipped": 2
  },
  "diff_items": [
    {
      "asset_path": "/Game/Art/Trees/T_Tree_01_D.T_Tree_01_D",
      "field": "max_texture_size",
      "before": 4096,
      "after": 1024,
      "tool_name": "SetTextureMaxSize",
      "risk": "write",
      "skip_reason": ""
    }
  ]
}
```
- 必填字段：`asset_path/field/before/after/tool_name/risk`
- 扩展字段（一期建议）：`object_type(asset/actor)`、`locate_strategy(camera_focus/sync_browser)`、`evidence_key`。
- UE 交互要求：
  - Diff 列表项必须支持定位资产；
  - 场景中可定位的 Actor：触发 `Camera Focus`；
  - 纯资产（`Texture2D/DataAsset` 等）：调用 `GEditor->SyncBrowserToObjects` 在 Content Browser 高亮。

#### 14.3.1 StageE-SliceE2 落地（最小预览链路）

- Runtime 新增：
  - `FHCIAbilityKitDryRunDiffReport / FHCIAbilityKitDryRunDiffItem / Summary`
  - `FHCIAbilityKitDryRunDiff::NormalizeAndFinalize(...)`
    - 自动汇总 `total_candidates/modifiable/skipped`
    - `object_type=actor -> locate_strategy=camera_focus`
    - `object_type=asset -> locate_strategy=sync_browser`
    - 自动补默认 `evidence_key`（`actor_path/asset_path`）
  - `FHCIAbilityKitDryRunDiffJsonSerializer`（输出 `request_id/summary/diff_items[]`）
- Editor 最小 UE 展示（控制台预览版，作为面板前置门禁）：
  - `HCIAbilityKit.DryRunDiffPreviewDemo`
    - 输出 Diff 摘要与 `row=` 列表（包含 `asset_path/field/before/after/tool_name/risk/skip_reason/object_type/locate_strategy/evidence_key`）
  - `HCIAbilityKit.DryRunDiffLocate [row_index]`
    - 按 `locate_strategy` 执行定位：
      - `camera_focus` -> 尝试定位 Actor 并聚焦
      - `sync_browser` -> 尝试加载对象并 `SyncBrowserToObjects`
  - `HCIAbilityKit.DryRunDiffPreviewJson`
    - 输出序列化后的 Dry-Run Diff JSON（用于契约核验）

### 14.4 安全执行策略契约（极简冻结）

- Blast Radius：
  - 固定常量：`MAX_ASSET_MODIFY_LIMIT = 50`；
  - 超阈值直接拦截并返回 `E4004`。
- 事务策略：
  - 固定 `All-or-Nothing`；
  - 任一步骤失败整单回滚并返回 `E4007`。
- SourceControl：
  - 当 `ISourceControlModule::Get().IsEnabled()==false`：进入“离线本地模式”，输出 Warning 并放行本地写操作；
  - 当 SourceControl 已启用：固定 `Fail-Fast`，`Checkout` 任一步失败立即终止并返回 `E4006`；
  - 一期不做自动重试与强制解锁。

#### 14.4.1 StageE-SliceE3 落地（确认门禁最小闭环）

- Runtime 新增确认门禁 helper：`FHCIAbilityKitAgentExecutionGate`
  - 输入：`FHCIAbilityKitAgentExecutionStep(step_id/tool_name/requires_confirm/user_confirmed)`
  - 输出：`FHCIAbilityKitAgentExecutionDecision(allowed/error_code/reason/capability/write_like/...)`
- 判定口径（一期最小实现）：
  - `tool_name` 不在 Tool Registry 白名单：返回 `E4002`
  - `capability=read_only`：允许执行（不要求确认）
  - `capability in {write, destructive}`：
    - `requires_confirm=false`：拦截并返回 `E4005`（`reason=write_step_requires_confirm`）
    - `requires_confirm=true && user_confirmed=false`：拦截并返回 `E4005`（`reason=user_not_confirmed`）
    - `requires_confirm=true && user_confirmed=true`：允许执行
- Editor UE 手测入口（控制台）：
  - `HCIAbilityKit.AgentConfirmGateDemo`
    - 默认输出三案例：`read_only_unconfirmed / write_unconfirmed / write_confirmed`
    - 输出摘要：`summary total_cases=... allowed=... blocked=... validation=ok`
  - `HCIAbilityKit.AgentConfirmGateDemo [tool_name] [requires_confirm 0|1] [user_confirmed 0|1]`
    - 用于单案例验证日志与错误码（如 `E4005`）

#### 14.4.2 StageE-SliceE4 落地（Blast Radius 极简限流）

- Runtime 在 `FHCIAbilityKitAgentExecutionGate` 中新增 Blast Radius 判定：
  - 固定常量：`MAX_ASSET_MODIFY_LIMIT = 50`
  - 输入：`request_id/tool_name/target_modify_count`
  - 输出：`allowed/error_code/reason/capability/write_like/target_modify_count/max_limit`
- 判定口径（一期最小实现）：
  - `tool_name` 不在 Tool Registry 白名单：返回 `E4002`
  - `capability=read_only`：不受 Blast Radius 限制，直接放行
  - `capability in {write, destructive}`：
    - `target_modify_count <= 50`：允许执行
    - `target_modify_count > 50`：拦截并返回 `E4004`（`reason=modify_limit_exceeded`）
- Editor UE 手测入口（控制台）：
  - `HCIAbilityKit.AgentBlastRadiusDemo`
    - 默认输出三案例：`read_only_large_count / write_at_limit / write_over_limit`
    - 输出摘要：`summary total_cases=... max_limit=50 ... validation=ok`
  - `HCIAbilityKit.AgentBlastRadiusDemo [tool_name] [target_modify_count>=0]`
    - 用于单案例验证 `E4004` 与边界值 `50`

#### 14.4.3 StageE-SliceE5 落地（All-or-Nothing 全单事务骨架）

- Runtime 在 `FHCIAbilityKitAgentExecutionGate` 中新增 All-or-Nothing 事务判定：
  - 输入：`request_id + steps[]`（每步含 `step_id/tool_name/should_succeed`，用于最小执行骨架模拟）
  - 输出：`committed/rolled_back/error_code/reason/transaction_mode/total_steps/executed_steps/committed_steps/rolled_back_steps/failed_step_*`
  - 固定 `transaction_mode=all_or_nothing`
- 判定口径（一期最小实现）：
  - 预检阶段（执行前）：
    - 任一步 `tool_name` 不在 Tool Registry 白名单：返回 `E4002`，`reason=tool_not_whitelisted`
    - 预检失败时 `executed_steps=0`，不得产生部分提交
  - 执行阶段（模拟最小骨架）：
    - 全部步骤成功：`committed=true`，`rolled_back=false`
    - 任一步失败：立即终止后续步骤，整单回滚并返回 `E4007`（`reason=step_failed_all_or_nothing_rollback`）
    - 失败时 `committed_steps=0`（无部分提交残留）
- Editor UE 手测入口（控制台）：
  - `HCIAbilityKit.AgentTransactionDemo`
    - 默认输出三案例：`all_success_commit / fail_step2_rollback / fail_step1_rollback`
    - 输出摘要：`summary total_cases=... committed=... rolled_back=... transaction_mode=all_or_nothing expected_failed_code=E4007 validation=ok`
  - `HCIAbilityKit.AgentTransactionDemo [total_steps>=1] [fail_step_index>=0]`
    - `fail_step_index=0` 表示全成功；`1..N` 表示第 N 步失败（1-based）
    - 用于单案例验证 `E4007` 与“无部分提交”日志口径
  - 案例日志最小字段：
    - `request_id`
    - `transaction_mode`
    - `total_steps/executed_steps/committed_steps/rolled_back_steps`
    - `committed/rolled_back`
    - `failed_step_index/failed_step_id/failed_tool_name`
    - `error_code/reason`

#### 14.4.4 StageE-SliceE6 落地（SourceControl Fail-Fast + 离线本地模式）

- Runtime 在 `FHCIAbilityKitAgentExecutionGate` 中新增 SourceControl 判定：
  - 输入：`request_id/tool_name/source_control_enabled/checkout_succeeded`（Editor 层负责真实 SC 状态采集；Runtime 仅做纯判定）
  - 输出：`allowed/error_code/reason/capability/write_like/source_control_enabled/fail_fast/offline_local_mode/checkout_attempted/checkout_succeeded`
- 判定口径（一期最小实现）：
  - `tool_name` 不在 Tool Registry 白名单：返回 `E4002`（`reason=tool_not_whitelisted`）
  - `capability=read_only`：允许执行，且不触发 `Checkout`（`checkout_attempted=false`）
  - `capability in {write, destructive}`：
    - `source_control_enabled=false`：进入“离线本地模式”，输出 Warning 并放行（`allowed=true`, `offline_local_mode=true`）
    - `source_control_enabled=true && checkout_succeeded=false`：`Fail-Fast` 拦截并返回 `E4006`（`reason=source_control_checkout_failed_fail_fast`）
    - `source_control_enabled=true && checkout_succeeded=true`：允许执行（`checkout_attempted=true`, `checkout_succeeded=true`）
  - 一期不做自动重试与强制解锁。
- Editor UE 手测入口（控制台）：
  - `HCIAbilityKit.AgentSourceControlDemo`
    - 默认输出三案例：`read_only_enabled_bypass / write_offline_local_mode / write_checkout_fail_fast`
    - 输出摘要：`summary total_cases=... offline_local_mode_cases=... fail_fast=true expected_blocked_code=E4006 validation=ok`
  - `HCIAbilityKit.AgentSourceControlDemo [tool_name] [source_control_enabled 0|1] [checkout_succeeded 0|1]`
    - 用于单案例验证离线放行、Fail-Fast 拦截与成功放行日志口径
  - 案例日志最小字段：
    - `request_id/tool_name/capability/write_like`
    - `source_control_enabled/fail_fast/offline_local_mode`
    - `checkout_attempted/checkout_succeeded`
    - `allowed/error_code/reason`

### 14.5 本地 Mock 权限与日志契约（极简冻结）

- 权限来源：本地用户名 + 本地角色映射文件（JSON）。
- 角色映射文件路径（一期固定）：`SourceData/AbilityKits/Config/agent_rbac_mock.json`
- 最小权限结构：
```json
{
  "user": "artist_a",
  "allowed_capabilities": [
    "read_only",
    "write"
  ]
}
```
- 默认策略（冻结）：
  - 未命中白名单用户默认映射为 `Guest`；
  - `Guest` 仅允许 `read_only`，阻断所有 `write/destructive`（返回 `E4008`）；
  - `Guest` 允许执行扫描与报告导出。
- Runtime E7 最小判定输出（`FHCIAbilityKitAgentExecutionGate::EvaluateMockRbac`）：
  - 输入：`request_id/user_name/resolved_role/user_matched_whitelist/tool_name/asset_count/allowed_capabilities[]`
  - 输出：`allowed/error_code/reason/user/resolved_role/user_in_whitelist/guest_fallback/tool_name/capability/write_like/asset_count`
  - 判定口径：
    - `tool_name` 不在 Tool Registry 白名单：返回 `E4002`（`reason=tool_not_whitelisted`）
    - `capability` 在 `allowed_capabilities[]` 中：允许执行（`reason=rbac_allowed`）
    - `Guest + read_only`：允许执行（`reason=guest_read_only_allowed`）
    - 其余权限不足：返回 `E4008`
      - `Guest` 写操作：`reason=guest_read_only_write_blocked`
      - 非 Guest 能力不足：`reason=capability_not_allowed_by_role`
- 审计日志落地：本地 `json/txt` 文件。
- 审计日志路径（一期固定）：`Saved/HCIAbilityKit/Audit/agent_exec_log.jsonl`
- 日志最小字段：`timestamp/user/request_id/tool_name/asset_count/result/error_code`。
- 时间显示兼容说明（2026-02-22 起）：
  - 对外日志/JSON 时间字符串统一按北京时间 ISO-8601（`+08:00`）输出；
  - 字段名为兼容既有门禁暂不重命名（例如 `updated_utc/generated_utc/timestamp_utc` 仍保留）。
- 本期 JSONL 序列化字段（E7 落地）：
  - `timestamp_utc`
  - `user`
  - `role`
  - `request_id`
  - `tool_name`
  - `capability`
  - `asset_count`
  - `result`（`allowed|blocked`）
  - `error_code`
  - `reason`
- Editor UE 手测入口（控制台）：
  - `HCIAbilityKit.AgentRbacDemo`
    - 默认输出三案例：`guest_read_only_allowed / guest_write_blocked / configured_write_allowed`
    - 同时写入本地审计日志并输出摘要：
      - `summary total_cases=... allowed=... blocked=... guest_fallback_cases=... audit_log_appends=... config_users=... validation=ok`
      - `paths config_path=... audit_log_path=...`
  - `HCIAbilityKit.AgentRbacDemo [user_name] [tool_name] [asset_count>=0]`
    - 用于单案例验证 `E4008`、Guest 回退与日志落盘口径
- 案例日志最小字段（E7）：
  - `request_id/user/resolved_role/user_in_whitelist/guest_fallback`
  - `tool_name/capability/write_like`
  - `asset_count`
  - `allowed/error_code/reason`
  - `audit_log_appended/audit_log_path/audit_log_error`

### 14.5.1 StageF-SliceF3 执行器骨架与收敛日志（最小落地）

- 目标：在不触发真实资产写操作的前提下，完成 `Plan -> Executor -> 收敛结果日志` 最小闭环。
- Runtime 新增执行器骨架：`FHCIAbilityKitAgentExecutor`
  - 输入：
    - `plan`（`FHCIAbilityKitAgentPlan`）
    - `tool_registry`
    - `validation_context`（复用 F2，可选）
    - `options`
      - `bValidatePlanBeforeExecute`（一期默认 `true`）
      - `bDryRun`（一期默认 `true`）
  - 输出：`FHCIAbilityKitAgentExecutorRunResult`
    - 顶层字段（一期冻结最小集）：
      - `request_id/intent/plan_version`
      - `execution_mode`（一期默认 `simulate_dry_run`）
      - `accepted/completed`
      - `total_steps/executed_steps/succeeded_steps/failed_steps`
      - `terminal_status/terminal_reason`
      - `error_code/reason`
      - `started_utc/finished_utc`（字符串值按北京时间 `+08:00` 输出）
      - `step_results[]`
    - `step_results[]` 字段（一期冻结最小集）：
      - `step_index/step_id/tool_name`
      - `capability/risk_level/write_like`
      - `attempted/succeeded/status`
      - `target_count_estimate`
      - `evidence_keys[]`
      - `evidence`（键值映射；一期为模拟证据）
      - `error_code/reason`
- 判定/执行口径（一期最小实现）：
  - 预检阶段：
    - 默认先调用 `FHCIAbilityKitAgentPlanValidator`。
    - 若预检失败：返回 `accepted=false, completed=false`，`terminal_status=rejected_precheck`，并透传 `error_code/reason`（例如 `E4002`）。
  - 执行阶段（模拟骨架）：
    - 不调用真实 UE 写操作工具，仅按 `ToolRegistry` 能力与 `expected_evidence` 生成模拟成功结果；
    - 默认所有步骤 `status=succeeded`，`terminal_status=completed`；
    - `execution_mode=simulate_dry_run`，用于后续 F4/F5 与真实门禁链路联调。
- Editor UE 手测入口（控制台）：
  - `HCIAbilityKit.AgentExecutePlanDemo`
    - 默认输出 3 案例（命名溯源整理 / 关卡排雷 / 资产规范合规）
    - 每案例输出：
      - `route_reason + summary`
      - `row=`（逐步执行结果）
    - 摘要输出：`summary total_cases=... completed_cases=... rejected_cases=... execution_mode=simulate_dry_run ... validation=ok`
  - `HCIAbilityKit.AgentExecutePlanDemo [自然语言文本...]`
    - 用于单案例验证 NL -> Plan -> Executor 收敛日志链路
- 一期说明（与 F4 边界）：
  - F3 仅做“成功路径骨架 + 预检拒绝路径”；
  - 步骤级失败错误码收敛与终止策略扩展放到 `StageF-SliceF4`。

### 14.5.2 StageF-SliceF4 失败收敛（步骤级错误码与终止策略）

- 目标：在 `simulate_dry_run` 执行骨架中补齐步骤级失败收敛、终止策略与可追踪失败证据，不触发真实资产写操作。
- Runtime 扩展：`FHCIAbilityKitAgentExecutor`
  - `options` 新增（F4 最小集）：
    - `termination_policy`
      - `stop_on_first_failure`
      - `continue_on_failure`
    - `SimulatedFailureStepIndex`（测试/演示 seam，`INDEX_NONE` 表示不注入失败）
    - `SimulatedFailureErrorCode`
    - `SimulatedFailureReason`
  - `run_result` 顶层新增字段：
    - `termination_policy`
    - `skipped_steps`
    - `failed_step_index/failed_step_id/failed_tool_name`（F3 已有字段在 F4 要求日志输出）
- 失败收敛口径（冻结）：
  - 预检拒绝路径保持 F3 不变：`terminal_status=rejected_precheck`。
  - 执行阶段单步失败（含模拟失败或运行时工具不可用）：
    - 步骤行：
      - `status=failed`
      - `attempted=true`
      - `succeeded=false`
      - `error_code/reason` 必填
    - 顶层：
      - 首个失败步骤写入 `failed_step_index/failed_step_id/failed_tool_name`
      - 顶层 `error_code/reason` 收敛为首个失败步骤错误
  - `termination_policy=stop_on_first_failure`
    - 首个失败后立即终止
    - 后续步骤仍需输出 `row=`，但标记：
      - `status=skipped`
      - `attempted=false`
      - `succeeded=false`
      - `reason=terminated_by_stop_on_first_failure`
    - 顶层：
      - `terminal_status=failed`
      - `terminal_reason=executor_step_failed_stop_on_first_failure`
  - `termination_policy=continue_on_failure`
    - 失败步骤后继续执行后续步骤
    - 顶层：
      - `terminal_status=completed_with_failures`
      - `terminal_reason=executor_step_failed_continue_on_failure`
      - `executed_steps=total_steps`
- Editor UE 手测入口（控制台）：
  - `HCIAbilityKit.AgentExecutePlanFailDemo`
    - 默认输出三案例：`ok_stop_on_first / fail_step1_stop_on_first / fail_step1_continue`
    - 输出：
      - F4 案例摘要（`summary total_cases=... stop_policy_cases=... continue_policy_cases=... validation=ok`）
      - 每案例 `AgentExecutor` 的 `summary + row=`
  - `HCIAbilityKit.AgentExecutePlanFailDemo [ok|fail_stop|fail_continue]`
    - 用于单案例验证步骤级失败与终止策略
- F4 `AgentExecutor` 日志字段（新增要求）：
  - `summary` 行新增（至少）：
    - `termination_policy`
    - `skipped_steps`
    - `failed_step_index/failed_step_id/failed_tool_name`
  - `row=` 行保持 F3 字段不删减，并在失败/跳过场景输出：
    - `status`
    - `attempted/succeeded`
    - `error_code/reason`

### 14.5.3 StageF-SliceF5 Executor 预检门禁链路接入（Stage E Gate Chain -> Executor）

- 目标：在 `FHCIAbilityKitAgentExecutor` 中把 Stage E 已冻结的安全门禁按步骤串联到执行前（预检链路），并把阻断结果收敛到 `summary + row=`，保持 `simulate_dry_run`，不触发真实资产写入。
- 预检链路顺序（冻结）：
  1. `ConfirmGate`
  2. `BlastRadius`
  3. `MockRBAC`
  4. `SourceControl Fail-Fast / OfflineLocalMode`
  5. `LOD Tool Safety`（仅 `SetMeshLODGroup` 生效）
- Runtime 扩展：`FHCIAbilityKitAgentExecutor`
  - `options` 新增（F5 最小集）：
    - `bEnablePreflightGates`
    - `bUserConfirmedWriteSteps`
    - `MockUserName / MockResolvedRole / bMockUserMatchedWhitelist / MockAllowedCapabilities`
    - `bSourceControlEnabled / bSourceControlCheckoutSucceeded`
    - `SimulatedLodTargetObjectClass / bSimulatedLodTargetNaniteEnabled`
  - `run_result` 顶层新增字段：
    - `preflight_enabled`
    - `preflight_blocked_steps`
    - `failed_gate`
  - `step_result` 新增字段：
    - `failure_phase`（`- / preflight / execute`）
    - `preflight_gate`（`- / passed / confirm_gate / blast_radius / rbac / source_control / lod_safety`）
- F5 失败收敛口径（冻结）：
  - 预检门禁阻断的步骤行：
    - `status=failed`
    - `attempted=true`
    - `succeeded=false`
    - `failure_phase=preflight`
    - `preflight_gate=<具体门禁>`
    - `error_code/reason` 直接透传对应 Stage E 门禁错误码（如 `E4005/E4004/E4008/E4006/E4010`）
  - 顶层收敛：
    - `failed_gate` 收敛为首个失败步骤的 `preflight_gate`
    - `preflight_blocked_steps` 统计所有被预检阻断的步骤数
    - 若 `termination_policy=stop_on_first_failure` 且失败来自预检：
      - `terminal_status=failed`
      - `terminal_reason=executor_preflight_gate_failed_stop_on_first_failure`
    - 若 `termination_policy=continue_on_failure` 且存在预检阻断：
      - `terminal_status=completed_with_failures`
      - `terminal_reason=executor_preflight_gate_failed_continue_on_failure`
  - 当 `bEnablePreflightGates=false`：
    - F3/F4 既有行为保持不变（兼容旧门禁日志口径）
- Editor UE 手测入口（控制台）：
  - `HCIAbilityKit.AgentExecutePlanPreflightDemo`
    - 默认输出 6 案例：`ok / fail_confirm / fail_blast / fail_rbac / fail_sc / fail_lod`
    - 每案例输出：
      - `[HCIAbilityKit][AgentExecutorPreflight] case=...`
      - `AgentExecutor summary + row=`
    - 总摘要输出（F5）：
      - `summary total_cases=... passed_cases=... blocked_cases=...`
      - `preflight_enabled=true`
      - `confirm_blocked / blast_radius_blocked / rbac_blocked / source_control_blocked / lod_safety_blocked`
      - `execution_mode=simulate_dry_run`
      - `validation=ok`
  - `HCIAbilityKit.AgentExecutePlanPreflightDemo [ok|fail_confirm|fail_blast|fail_rbac|fail_sc|fail_lod]`
    - 用于单案例验证某个门禁在 Executor 预检链路中的收敛口径

### 14.5.4 StageF-SliceF6 Executor -> Dry-Run Diff 审阅桥接（执行结果进入审阅契约）

- 目标：将 `FHCIAbilityKitAgentExecutorRunResult(step_results[])` 转换为 Stage E2 已冻结的 `FHCIAbilityKitDryRunDiffReport`，打通 `计划 -> 执行 -> 审阅` 的最小闭环（仍为 `simulate_dry_run`，不触发真实资产写入）。
- Runtime 新增桥接器（F6）：
  - `FHCIAbilityKitAgentExecutorDryRunBridge::BuildDryRunDiffReport(...)`
  - 输入：`FHCIAbilityKitAgentExecutorRunResult`
  - 输出：`FHCIAbilityKitDryRunDiffReport`
- F6 映射规则（冻结）：
  - 顶层：
    - `report.request_id <- run_result.request_id`
    - `report.summary <- 通过 FHCIAbilityKitDryRunDiff::NormalizeAndFinalize(...) 统一计算`
  - 行级（`step_result -> diff_item`）：
    - `tool_name <- step_result.tool_name`
    - `risk <- step_result.risk_level` 映射到 `read_only|write|destructive`
    - `field <- "step:<step_id>"`（缺失 `step_id` 时退化为 `executor_step_status`）
    - `asset_path` 优先级：
      1. `evidence.actor_path`（同时设置 `actor_path`，并令 `object_type=actor`）
      2. `evidence.asset_path`
      3. `"-"`（占位）
    - `evidence_key`：
      - Actor 行：`actor_path`
      - Asset 行：`asset_path`
      - 无路径证据：`result`
    - `before/after`：
      - 优先使用 `evidence.before/evidence.after`
      - 若缺失 `after`，退化为 `step_result.status`
    - `skip_reason`（仅失败/跳过行写入）：
      - 收敛 `error_code/failure_phase/preflight_gate/reason`
      - 用于审阅列表标记“阻断/跳过原因”
  - 定位策略：
    - 仍由 `FHCIAbilityKitDryRunDiff::NormalizeAndFinalize(...)` 推导：
      - `object_type=actor -> camera_focus`
      - `object_type=asset -> sync_browser`
- 兼容性要求（F6）：
  - 不修改 F3/F4/F5 `AgentExecutor summary + row=` 既有字段口径（仅新增 F6 专用命令输出）。
  - 不修改 E2 `DryRunDiff` JSON 核心字段名（F6 复用 `request_id/summary/diff_items[]` 契约与 `locate_strategy/evidence_key`）。
- Editor UE 手测入口（控制台）：
  - `HCIAbilityKit.AgentExecutePlanReviewDemo`
    - 默认输出 3 案例：`ok_naming / ok_level_risk / fail_confirm`
    - 每案例输出：
      - `AgentExecutor summary + row=`（原 F3/F4/F5 收敛日志）
      - `AgentExecutorReview summary + row=`（桥接后的 Dry-Run Diff 审阅日志）
    - 总摘要字段（F6 最小门禁）：
      - `summary total_cases=...`
      - `bridge_ok_cases=...`
      - `failed_cases=...`
      - `execution_mode=simulate_dry_run`
      - `review_contract=dry_run_diff`
      - `validation=ok`
  - `HCIAbilityKit.AgentExecutePlanReviewDemo [ok_naming|ok_level_risk|fail_confirm|fail_lod]`
    - 用于单案例验证审阅桥接结果（Actor 定位 / 预检阻断映射等）
  - `HCIAbilityKit.AgentExecutePlanReviewJson [ok_naming|ok_level_risk|fail_confirm|fail_lod]`
    - 输出桥接后的 `DryRunDiff` JSON（复用 E2 序列化器）
- F6 审阅日志字段（新增要求）：
  - `summary` 行至少包含：
    - `request_id`
    - `total_candidates/modifiable/skipped`
    - `executor_terminal_status`
    - `executor_failed_gate`
    - `execution_mode`
    - `bridge_ok=true`
  - `row=` 行至少包含：
    - `asset_path/field/before/after/tool_name/risk/skip_reason`
    - `object_type/locate_strategy/evidence_key`
    - `actor_path`（Actor 行存在）

### 14.5.5 StageF-SliceF7 ExecutorReview 定位闭环（审阅桥接结果按行定位）

- 目标：在 F6 生成的 `FHCIAbilityKitDryRunDiffReport`（ExecutorReview 预览状态）上提供按行定位命令，打通 `计划 -> 执行 -> 审阅 -> 定位` 的最小交互闭环（仍为 `simulate_dry_run`，不触发真实资产写入）。
- Editor 新增命令（F7）：
  - `HCIAbilityKit.AgentExecutePlanReviewLocate [row_index]`
  - 输入来源：F6 预览状态 `GHCIAbilityKitAgentExecutorReviewDiffPreviewState`
- F7 定位解析 helper（可测，冻结）：
  - `FHCIAbilityKitAgentExecutorReviewLocateUtils::TryResolveRow(...)`
  - 行为：
    - `DiffItems.Num()==0` -> `reason=no_preview_state`
    - `row_index<0 || out_of_range` -> `reason=row_index_out_of_range`
    - 成功 -> 输出解析后的 `row_index/asset_path/actor_path/tool_name/object_type/locate_strategy`
- F7 实际定位行为（Editor）：
  - `locate_strategy=camera_focus`：
    - 优先定位 `actor_path`
    - 若 `actor_path` 为空则回退使用 `asset_path`
    - 失败原因示例：`g_editor_unavailable / editor_world_unavailable / actor_not_found`
  - `locate_strategy=sync_browser`：
    - 尝试 `FSoftObjectPath.ResolveObject()`，失败后 `TryLoad()`
    - 成功后 `GEditor->SyncBrowserToObjects(...)`
    - 失败原因示例：`g_editor_unavailable / asset_load_failed`
- F7 日志口径（新增，冻结）：
  - 无预览状态（Warning + 指引）：
    - `[HCIAbilityKit][AgentExecutorReview] locate=unavailable reason=no_preview_state`
    - `suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewDemo 或 HCIAbilityKit.AgentExecutePlanReviewJson`
  - 非法参数（Error）：
    - `locate_invalid_args reason=row_index must be integer >= 0`
    - 或 `locate_invalid_args reason=row_index_out_of_range row_index=... total=...`
  - 定位结果（Display/Warning）：
    - `locate row=... tool_name=... strategy=... object_type=... success=true|false reason=... asset_path=... actor_path=...`
- 兼容性要求（F7）：
  - 不修改 F6 `AgentExecutePlanReviewDemo/Json` 的既有 `summary + row=` 字段口径。
  - F7 仅新增 `Locate` 命令与辅助 util，不改变 F6 桥接 JSON 契约。

### 14.5.6 StageF-SliceF8 ExecutorReview 逐项采纳选择契约与过滤预览（Selection）

- 目标：在 F6/F7 已建立的“最新审阅预览”`FHCIAbilityKitDryRunDiffReport` 上，按 `row_index` 列表执行逐项采纳选择（去重/越界校验/过滤），生成“已采纳子集”审阅预览与 JSON（仍为 `simulate_dry_run`，不触发真实资产写入）。
- Runtime 新增选择过滤器（F8）：
  - `FHCIAbilityKitDryRunDiffSelection::SelectRows(...)`
  - 输入：`SourceReport + RequestedRowIndices`
  - 输出：`FHCIAbilityKitDryRunDiffSelectionResult`
- F8 选择过滤规则（冻结）：
  - `RequestedRowIndices.Num()==0`：失败，`error_code=E4201`，`reason=empty_row_selection`
  - 任一 `row_index < 0 || out_of_range`：失败，`error_code=E4201`，`reason=row_index_out_of_range`
  - 重复行号：允许输入，但执行去重（按“首次出现顺序”保留）
    - 统计 `dropped_duplicates`
  - 去重后为空：失败，`error_code=E4201`，`reason=empty_row_selection_after_dedup`
  - 成功：输出 `SelectedReport`，并复用 `FHCIAbilityKitDryRunDiff::NormalizeAndFinalize(...)` 重新计算 `summary/modifiable/skipped` 与定位策略归一化
- F8 结果结构（最小字段，冻结）：
  - `bSuccess/error_code/reason`
  - `input_row_count/unique_row_count/dropped_duplicate_rows`
  - `total_rows_before/total_rows_after`
  - `applied_row_indices[]`
  - `selected_report`
- Editor 新增命令（F8）：
  - `HCIAbilityKit.AgentExecutePlanReviewSelect [row_indices_csv]`
  - `HCIAbilityKit.AgentExecutePlanReviewSelectJson [row_indices_csv]`
  - 输入来源：F6/F7 预览状态 `GHCIAbilityKitAgentExecutorReviewDiffPreviewState`
  - 行号参数兼容：逗号分隔或空格分隔（如 `0,2` / `0 2`）
  - 成功后行为：将“最新审阅预览状态”替换为已采纳子集（便于直接复用 F7 `Locate`）
- F8 日志口径（新增，冻结）：
  - 无预览状态（Warning + 指引）：
    - `[HCIAbilityKit][AgentExecutorReviewSelect] select=unavailable reason=no_preview_state`
    - `suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewDemo 或 HCIAbilityKit.AgentExecutePlanReviewJson`
  - 非法参数（Error）：
    - `invalid_args reason=row_index must be integer >= 0 ...`
    - 或 `invalid_args error_code=E4201 reason=row_index_out_of_range total=...`
  - 选择摘要（Display/Warning）：
    - `[HCIAbilityKit][AgentExecutorReviewSelect] summary request_id=... total_before=... total_after=... input_rows=... unique_rows=... dropped_duplicates=... modifiable=... skipped=... validation=ok`
  - 选择结果行（Display/Warning）：
    - `row=... asset_path=... field=... before=... after=... tool_name=... risk=... skip_reason=... object_type=... locate_strategy=... evidence_key=... actor_path=...`
  - JSON 输出：
    - `[HCIAbilityKit][AgentExecutorReviewSelect] json request_id=... bytes=...`
    - 后续输出 JSON 文本（复用 E2 `DryRunDiff` 序列化器）
- 兼容性要求（F8）：
  - 不修改 F6 `AgentExecutePlanReviewDemo/Json` 与 F7 `AgentExecutePlanReviewLocate` 的既有日志字段口径。
  - F8 仅新增“选择过滤”命令与 Runtime 过滤器；输出 JSON 仍复用 E2 `DryRunDiff` 契约字段名（`request_id/summary/diff_items[]`）。

### 14.5.7 StageF-SliceF9 ExecutorReview 采纳子集 -> ApplyRequest 执行申请包桥接

- 目标：将 F8 生成的“已采纳子集”`FHCIAbilityKitDryRunDiffReport` 桥接为可确认、可追踪的 `ApplyRequest` 契约与 JSON，为后续 F10“确认后执行”提供稳定输入（仍为 `simulate_dry_run`，不触发真实资产写入）。
- Runtime 新增桥接器（F9）：
  - `FHCIAbilityKitAgentExecutorApplyRequestBridge::BuildApplyRequest(...)`
  - 输入：`SelectedReviewReport (DryRunDiffReport)`
  - 输出：`FHCIAbilityKitAgentApplyRequest`
- F9 `ApplyRequest` 顶层字段（最小冻结）：
  - `request_id`：F9 申请包请求号（可与审阅请求号不同）
  - `review_request_id`：来源审阅预览请求号（来自 F6/F8 `DryRunDiffReport.request_id`）
  - `selection_digest`：对已采纳子集行内容生成的稳定摘要（用于后续确认执行时校验“审阅未被篡改”）
  - `generated_utc`：时间字符串（值按北京时间 `+08:00` 输出，字段名兼容保留）
  - `execution_mode`：一期固定 `simulate_dry_run_apply_request`
  - `ready`：是否可进入确认执行（`blocked_rows==0` 时为 `true`）
  - `summary`
  - `items[]`
- F9 `summary` 字段（最小冻结）：
  - `total_rows`
  - `modifiable_rows`
  - `blocked_rows`
  - `read_only_rows`
  - `write_rows`
  - `destructive_rows`
  - `selected_rows`
- F9 `items[]` 字段（最小冻结）：
  - `row_index`
  - `tool_name`
  - `risk`
  - `asset_path`
  - `field`
  - `skip_reason`
  - `blocked`
  - `object_type`
  - `locate_strategy`
  - `evidence_key`
  - `actor_path`（可选）
- F9 桥接规则（冻结）：
  - `DryRunDiffItem.skip_reason` 非空 -> `blocked=true`
  - `blocked_rows > 0` -> `ready=false`
  - `blocked_rows == 0` -> `ready=true`
  - `selection_digest` 必须对“行顺序 + 核心字段”稳定计算；相同输入报告应生成相同摘要值
- Editor 新增命令（F9）：
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareApply`
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareApplyJson`
  - 输入来源：F6/F7/F8 “最新审阅预览状态” `GHCIAbilityKitAgentExecutorReviewDiffPreviewState`
  - 成功后：缓存“最新 ApplyRequest 预览状态”（供后续 F10 使用）
- F9 日志口径（新增，冻结）：
  - 无预览状态（Warning + 指引）：
    - `[HCIAbilityKit][AgentExecutorApply] prepare=unavailable reason=no_review_preview_state`
    - `suggestion=先运行 HCIAbilityKit.AgentExecutePlanReviewDemo / ...ReviewSelect`
  - 申请包摘要（Display/Warning）：
    - `[HCIAbilityKit][AgentExecutorApply] summary request_id=... review_request_id=... selection_digest=... total_rows=... modifiable_rows=... blocked_rows=... ready=true|false validation=ok`
  - 申请包行（Display/Warning）：
    - `row=... tool_name=... risk=... blocked=true|false skip_reason=... object_type=... locate_strategy=... asset_path=... field=... evidence_key=... actor_path=...`
  - JSON 输出：
    - `[HCIAbilityKit][AgentExecutorApply] json request_id=... bytes=...`
    - 后续输出 JSON 文本（F9 `ApplyRequest` 契约）
- 兼容性要求（F9）：
  - 不修改 F6/F7/F8 现有命令与日志字段口径。
  - F9 仅做“审阅采纳子集 -> 执行申请包”桥接与 JSON 输出，不触发真实写资产/事务提交。

### 14.5.8 StageF-SliceF10 ApplyRequest 确认前校验 -> ConfirmRequest 执行确认申请桥接

- 目标：基于 F9 的 `ApplyRequest` 与最新审阅预览执行确认前校验（`review_request_id + selection_digest + ready + user_confirmed`），桥接为 `ConfirmRequest` 契约与 JSON，为后续 F11“确认后执行骨架（仍 dry-run）”提供稳定输入。
- Runtime 新增桥接器（F10）：
  - `FHCIAbilityKitAgentExecutorApplyConfirmBridge::BuildConfirmRequest(...)`
  - 输入：`ApplyRequest`（来自 F9） + `CurrentReviewReport`（F6/F8 最新审阅预览）+ `user_confirmed`
  - 输出：`FHCIAbilityKitAgentApplyConfirmRequest`
- F10 `ConfirmRequest` 顶层字段（最小冻结）：
  - `request_id`
  - `apply_request_id`
  - `review_request_id`
  - `selection_digest`
  - `generated_utc`（值按北京时间 `+08:00` 输出，字段名兼容保留）
  - `execution_mode`（一期固定：`simulate_dry_run_apply_confirm_request`）
  - `user_confirmed`
  - `ready_to_execute`
  - `error_code`（成功可为 `-`）
  - `reason`
  - `summary`
  - `items[]`
- F10 `summary` 字段（最小冻结）：
  - `total_rows`
  - `modifiable_rows`
  - `blocked_rows`
  - `read_only_rows`
  - `write_rows`
  - `destructive_rows`
  - `selected_rows`
- F10 `items[]` 字段（最小冻结）：
  - 复用 F9 `ApplyRequest.items[]` 字段集：
    - `row_index/tool_name/risk/asset_path/field/skip_reason/blocked/object_type/locate_strategy/evidence_key/actor_path`
- F10 校验规则（冻结）：
  - `user_confirmed=false` -> `ready_to_execute=false`，`error_code=E4005`，`reason=user_not_confirmed`
  - `ApplyRequest.ready=false` 或 `ApplyRequest.summary.blocked_rows>0` -> `ready_to_execute=false`，`error_code=E4203`，`reason=apply_request_not_ready_blocked_rows_present`
  - `ApplyRequest.review_request_id != CurrentReviewReport.request_id` -> `ready_to_execute=false`，`error_code=E4202`，`reason=review_request_id_mismatch`
  - `ApplyRequest.selection_digest` 与 `CurrentReviewReport` 重算摘要不一致 -> `ready_to_execute=false`，`error_code=E4202`，`reason=selection_digest_mismatch`
  - 全部通过 -> `ready_to_execute=true`，`error_code=-`，`reason=confirm_request_ready`
- Editor 新增命令（F10）：
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareConfirm [user_confirmed=0|1] [tamper=none|digest|review]`
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareConfirmJson [user_confirmed=0|1] [tamper=none|digest|review]`
  - 输入来源：F9 “最新 ApplyRequest 预览状态” + F6/F8 “最新审阅预览状态”
  - `tamper` 仅用于 UE 手测模拟：
    - `none`：正常校验
    - `digest`：篡改 `selection_digest`（触发 `E4202/selection_digest_mismatch`）
    - `review`：篡改 `review_request_id`（触发 `E4202/review_request_id_mismatch`）
- F10 日志口径（新增，冻结）：
  - 无状态（Warning + 指引）：
    - `[HCIAbilityKit][AgentExecutorConfirm] prepare=unavailable reason=no_apply_request_preview_state`
    - `[HCIAbilityKit][AgentExecutorConfirm] prepare=unavailable reason=no_review_preview_state`
  - 非法参数（Error）：
    - `invalid_args usage=HCIAbilityKit.AgentExecutePlanReviewPrepareConfirm [user_confirmed=0|1] [tamper=none|digest|review]`
  - 摘要（Display/Warning）：
    - `[HCIAbilityKit][AgentExecutorConfirm] summary request_id=... apply_request_id=... review_request_id=... selection_digest=... user_confirmed=true|false ready_to_execute=true|false error_code=... reason=... validation=ok`
  - 行（Display/Warning）：
    - `row=... tool_name=... risk=... blocked=... skip_reason=... object_type=... locate_strategy=... asset_path=... field=... evidence_key=... actor_path=...`
  - JSON 输出：
    - `[HCIAbilityKit][AgentExecutorConfirm] json request_id=... bytes=...`
    - 后续输出 `ConfirmRequest` JSON 文本
- 兼容性要求（F10）：
  - 不修改 F6/F7/F8/F9 现有命令与日志字段口径。
  - F10 仅做“确认前校验 + ConfirmRequest 桥接/JSON 输出”，不触发真实写资产/事务提交。

### 14.5.9 StageF-SliceF11 ConfirmRequest 确认后校验 -> ExecuteTicket 执行票据桥接

- 目标：基于 F10 的 `ConfirmRequest` 与最新 `ApplyRequest/Review` 预览执行确认后完整性校验（`apply_request_id + review_request_id + selection_digest + ready_to_execute + user_confirmed`），桥接为可投递给后续执行器的 `ExecuteTicket` 契约与 JSON；仍为 `simulate_dry_run`。
- Runtime 新增桥接器（F11）：
  - `FHCIAbilityKitAgentExecutorExecuteTicketBridge::BuildExecuteTicket(...)`
  - 输入：`ConfirmRequest`（F10） + `CurrentApplyRequest`（F9） + `CurrentReviewReport`（F6/F8）
  - 输出：`FHCIAbilityKitAgentExecuteTicket`
- F11 `ExecuteTicket` 顶层字段（最小冻结）：
  - `request_id`
  - `confirm_request_id`
  - `apply_request_id`
  - `review_request_id`
  - `selection_digest`
  - `generated_utc`（值按北京时间 `+08:00` 输出，字段名兼容保留）
  - `execution_mode`（一期固定：`simulate_dry_run_execute_ticket`）
  - `transaction_mode`（一期固定：`all_or_nothing`）
  - `termination_policy`（一期固定：`stop_on_first_failure`）
  - `user_confirmed`
  - `ready_to_simulate_execute`
  - `error_code`（成功可为 `-`）
  - `reason`
  - `summary`
  - `items[]`
- F11 校验规则（冻结）：
  - `ConfirmRequest.user_confirmed=false` -> `ready_to_simulate_execute=false`，`error_code=E4005`，`reason=user_not_confirmed`
  - `ConfirmRequest.ready_to_execute=false` -> `ready_to_simulate_execute=false`，`error_code=E4204`，`reason=confirm_request_not_ready`
  - `ConfirmRequest.apply_request_id != CurrentApplyRequest.request_id` -> `ready_to_simulate_execute=false`，`error_code=E4202`，`reason=apply_request_id_mismatch`
  - `ConfirmRequest.review_request_id != CurrentReviewReport.request_id` -> `ready_to_simulate_execute=false`，`error_code=E4202`，`reason=review_request_id_mismatch`
  - `ConfirmRequest.selection_digest != CurrentApplyRequest.selection_digest` 或与 `CurrentReviewReport` 重算摘要不一致 -> `ready_to_simulate_execute=false`，`error_code=E4202`，`reason=selection_digest_mismatch`
  - 全部通过 -> `ready_to_simulate_execute=true`，`error_code=-`，`reason=execute_ticket_ready`
- Editor 新增命令（F11）：
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareExecuteTicket [tamper=none|digest|apply|review]`
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareExecuteTicketJson [tamper=none|digest|apply|review]`
  - 输入来源：F10 “最新 ConfirmRequest 预览状态” + F9 “最新 ApplyRequest 预览状态” + F6/F8 “最新审阅预览状态”
  - `tamper` 仅用于 UE 手测模拟：
    - `none`：正常校验
    - `digest`：篡改 `selection_digest`（触发 `E4202/selection_digest_mismatch`）
    - `apply`：篡改 `apply_request_id`（触发 `E4202/apply_request_id_mismatch`）
    - `review`：篡改 `review_request_id`（触发 `E4202/review_request_id_mismatch`）
- F11 日志口径（新增，冻结）：
  - 摘要（Display/Warning）：
    - `[HCIAbilityKit][AgentExecutorExecuteTicket] summary request_id=... confirm_request_id=... apply_request_id=... review_request_id=... selection_digest=... user_confirmed=true|false ready_to_simulate_execute=true|false error_code=... reason=... validation=ok`
  - 行（Display/Warning）：
    - `row=... tool_name=... risk=... blocked=... skip_reason=... object_type=... locate_strategy=... asset_path=... field=... evidence_key=... actor_path=...`
  - JSON 输出：
    - `[HCIAbilityKit][AgentExecutorExecuteTicket] json request_id=... bytes=...`

### 14.5.10 StageF-SliceF12 ExecuteTicket 执行前投递校验 -> SimExecuteReceipt 模拟执行回执桥接

- 目标：基于 F11 的 `ExecuteTicket` 与最新 `ConfirmRequest/ApplyRequest/Review` 预览执行投递前完整性校验（`confirm_request_id + apply_request_id + review_request_id + selection_digest + ready_to_simulate_execute + user_confirmed`），桥接为 `SimExecuteReceipt` 契约与 JSON；仍为 `simulate_dry_run`。
- Runtime 新增桥接器（F12）：
  - `FHCIAbilityKitAgentExecutorSimulateExecuteReceiptBridge::BuildSimulateExecuteReceipt(...)`
  - 输入：`ExecuteTicket`（F11） + `CurrentConfirmRequest`（F10） + `CurrentApplyRequest`（F9） + `CurrentReviewReport`（F6/F8）
  - 输出：`FHCIAbilityKitAgentSimulateExecuteReceipt`
- F12 `SimExecuteReceipt` 顶层字段（最小冻结）：
  - `request_id`
  - `execute_ticket_id`
  - `confirm_request_id`
  - `apply_request_id`
  - `review_request_id`
  - `selection_digest`
  - `generated_utc`（值按北京时间 `+08:00` 输出，字段名兼容保留）
  - `execution_mode`（一期固定：`simulate_dry_run_execute_receipt`）
  - `transaction_mode`（沿用 F11：`all_or_nothing`）
  - `termination_policy`（沿用 F11：`stop_on_first_failure`）
  - `user_confirmed`
  - `ready_to_simulate_execute`（透传 F11）
  - `simulated_dispatch_accepted`
  - `error_code`（成功可为 `-`）
  - `reason`
  - `summary`
  - `items[]`
- F12 校验规则（冻结）：
  - `ExecuteTicket.user_confirmed=false` -> `simulated_dispatch_accepted=false`，`error_code=E4005`，`reason=user_not_confirmed`
  - `ExecuteTicket.ready_to_simulate_execute=false` -> `simulated_dispatch_accepted=false`，`error_code=E4205`，`reason=execute_ticket_not_ready`
  - `CurrentConfirmRequest.ready_to_execute=false` -> `simulated_dispatch_accepted=false`，`error_code=E4204`，`reason=confirm_request_not_ready`
  - `ExecuteTicket.confirm_request_id != CurrentConfirmRequest.request_id` -> `simulated_dispatch_accepted=false`，`error_code=E4202`，`reason=confirm_request_id_mismatch`
  - `ExecuteTicket.apply_request_id != CurrentApplyRequest.request_id` -> `simulated_dispatch_accepted=false`，`error_code=E4202`，`reason=apply_request_id_mismatch`
  - `ExecuteTicket.review_request_id != CurrentReviewReport.request_id` -> `simulated_dispatch_accepted=false`，`error_code=E4202`，`reason=review_request_id_mismatch`
  - `ExecuteTicket.selection_digest` 与 `CurrentConfirmRequest/CurrentApplyRequest` 或 `CurrentReviewReport` 重算摘要任一不一致 -> `simulated_dispatch_accepted=false`，`error_code=E4202`，`reason=selection_digest_mismatch`
  - 全部通过 -> `simulated_dispatch_accepted=true`，`error_code=-`，`reason=simulate_execute_receipt_ready`
- Editor 新增命令（F12）：
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimExecuteReceipt [tamper=none|digest|apply|review|confirm|ready]`
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimExecuteReceiptJson [tamper=none|digest|apply|review|confirm|ready]`
  - 输入来源：F11 “最新 ExecuteTicket 预览状态” + F10 “最新 ConfirmRequest 预览状态” + F9 “最新 ApplyRequest 预览状态” + F6/F8 “最新审阅预览状态”
  - `tamper` 仅用于 UE 手测模拟：
    - `none`：正常校验
    - `digest`：篡改 `selection_digest`（触发 `E4202/selection_digest_mismatch`）
    - `apply`：篡改 `apply_request_id`（触发 `E4202/apply_request_id_mismatch`）
    - `review`：篡改 `review_request_id`（触发 `E4202/review_request_id_mismatch`）
    - `confirm`：篡改 `confirm_request_id`（触发 `E4202/confirm_request_id_mismatch`）
    - `ready`：强制清空 `ready_to_simulate_execute`（触发 `E4205/execute_ticket_not_ready`）
- F12 日志口径（新增，冻结）：
  - 摘要（Display/Warning）：
    - `[HCIAbilityKit][AgentExecutorSimExecuteReceipt] summary request_id=... execute_ticket_id=... confirm_request_id=... apply_request_id=... review_request_id=... selection_digest=... user_confirmed=true|false ready_to_simulate_execute=true|false simulated_dispatch_accepted=true|false error_code=... reason=... validation=ok`
  - 行（Display/Warning）：
    - `row=... tool_name=... risk=... blocked=... skip_reason=... object_type=... locate_strategy=... asset_path=... field=... evidence_key=... actor_path=...`
  - JSON 输出：
    - `[HCIAbilityKit][AgentExecutorSimExecuteReceipt] json request_id=... bytes=...`

### 14.5.11 StageF-SliceF13 SimExecuteReceipt 回执后完整性校验 -> SimFinalReport 最终模拟执行报告桥接

- 目标：基于 F12 的 `SimExecuteReceipt` 与最新 `ExecuteTicket/ConfirmRequest/ApplyRequest/Review` 预览执行回执后完整性校验（`execute_ticket_id + confirm_request_id + apply_request_id + review_request_id + selection_digest + ready_to_simulate_execute + simulated_dispatch_accepted + user_confirmed`），桥接为 `SimFinalReport` 契约与 JSON；仍为 `simulate_dry_run`。
- Runtime 新增桥接器（F13）：
  - `FHCIAbilityKitAgentExecutorSimulateExecuteFinalReportBridge::BuildSimulateExecuteFinalReport(...)`
  - 输入：`SimExecuteReceipt`（F12） + `CurrentExecuteTicket`（F11） + `CurrentConfirmRequest`（F10） + `CurrentApplyRequest`（F9） + `CurrentReviewReport`（F6/F8）
  - 输出：`FHCIAbilityKitAgentSimulateExecuteFinalReport`
- F13 `SimFinalReport` 顶层字段（最小冻结）：
  - `request_id`
  - `sim_execute_receipt_id`
  - `execute_ticket_id`
  - `confirm_request_id`
  - `apply_request_id`
  - `review_request_id`
  - `selection_digest`
  - `generated_utc`（值按北京时间 `+08:00` 输出，字段名兼容保留）
  - `execution_mode`（一期固定：`simulate_dry_run_final_report`）
  - `transaction_mode`（沿用 F11/F12：`all_or_nothing`）
  - `termination_policy`（沿用 F11/F12）
  - `terminal_status`（`completed|blocked`）
  - `user_confirmed`
  - `ready_to_simulate_execute`
  - `simulated_dispatch_accepted`
  - `simulation_completed`
  - `error_code`（成功可为 `-`）
  - `reason`
  - `summary`
  - `items[]`
- F13 校验规则（冻结）：
  - `SimExecuteReceipt.user_confirmed=false` -> `simulation_completed=false`，`error_code=E4005`，`reason=user_not_confirmed`
  - `SimExecuteReceipt.ready_to_simulate_execute=false` -> `simulation_completed=false`，`error_code=E4205`，`reason=execute_ticket_not_ready`
  - `SimExecuteReceipt.simulated_dispatch_accepted=false` -> `simulation_completed=false`，`error_code=E4206`，`reason=simulate_execute_receipt_not_accepted`
  - `CurrentExecuteTicket.ready_to_simulate_execute=false` -> `simulation_completed=false`，`error_code=E4205`，`reason=execute_ticket_not_ready`
  - `CurrentConfirmRequest.ready_to_execute=false` -> `simulation_completed=false`，`error_code=E4204`，`reason=confirm_request_not_ready`
  - `SimExecuteReceipt.execute_ticket_id != CurrentExecuteTicket.request_id` -> `simulation_completed=false`，`error_code=E4202`，`reason=execute_ticket_id_mismatch`
  - `SimExecuteReceipt.confirm_request_id != CurrentConfirmRequest.request_id` -> `simulation_completed=false`，`error_code=E4202`，`reason=confirm_request_id_mismatch`
  - `SimExecuteReceipt.apply_request_id != CurrentApplyRequest.request_id` -> `simulation_completed=false`，`error_code=E4202`，`reason=apply_request_id_mismatch`
  - `SimExecuteReceipt.review_request_id != CurrentReviewReport.request_id` -> `simulation_completed=false`，`error_code=E4202`，`reason=review_request_id_mismatch`
  - `SimExecuteReceipt.selection_digest` 与 `CurrentExecuteTicket/CurrentConfirmRequest/CurrentApplyRequest` 或 `CurrentReviewReport` 重算摘要任一不一致 -> `simulation_completed=false`，`error_code=E4202`，`reason=selection_digest_mismatch`
  - 全部通过 -> `simulation_completed=true`，`terminal_status=completed`，`error_code=-`，`reason=simulate_execute_final_report_ready`
- Editor 新增命令（F13）：
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimFinalReport [tamper=none|digest|apply|review|confirm|receipt|ready]`
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimFinalReportJson [tamper=none|digest|apply|review|confirm|receipt|ready]`
  - 输入来源：F12 “最新 SimExecuteReceipt 预览状态” + F11/F10/F9/F6-F8 最新预览状态
  - `tamper` 仅用于 UE 手测模拟：
    - `none`：正常校验
    - `digest`：篡改 `selection_digest`（触发 `E4202/selection_digest_mismatch`）
    - `apply`：篡改 `apply_request_id`（触发 `E4202/apply_request_id_mismatch`）
    - `review`：篡改 `review_request_id`（触发 `E4202/review_request_id_mismatch`）
    - `confirm`：篡改 `confirm_request_id`（触发 `E4202/confirm_request_id_mismatch`）
    - `receipt`：强制 `simulated_dispatch_accepted=false`（触发 `E4206/simulate_execute_receipt_not_accepted`）
    - `ready`：强制 `ready_to_simulate_execute=false`（触发 `E4205/execute_ticket_not_ready`）
- F13 日志口径（新增，冻结）：
  - 摘要（Display/Warning）：
    - `[HCIAbilityKit][AgentExecutorSimFinalReport] summary request_id=... sim_execute_receipt_id=... execute_ticket_id=... confirm_request_id=... apply_request_id=... review_request_id=... selection_digest=... user_confirmed=true|false ready_to_simulate_execute=true|false simulated_dispatch_accepted=true|false simulation_completed=true|false terminal_status=... error_code=... reason=... validation=ok`
  - 行（Display/Warning）：
    - `row=... tool_name=... risk=... blocked=... skip_reason=... object_type=... locate_strategy=... asset_path=... field=... evidence_key=... actor_path=...`
  - JSON 输出：
    - `[HCIAbilityKit][AgentExecutorSimFinalReport] json request_id=... bytes=...`

### 14.5.12 StageF-SliceF14 SimFinalReport 最终模拟执行报告完整性校验 -> SimArchiveBundle 最终模拟存证包桥接

- 目标：基于 F13 的 `SimFinalReport` 与最新 `SimExecuteReceipt/ExecuteTicket/ConfirmRequest/ApplyRequest/Review` 预览执行终态完整性校验（`sim_execute_receipt_id + execute_ticket_id + confirm_request_id + apply_request_id + review_request_id + selection_digest + ready_to_simulate_execute + simulated_dispatch_accepted + simulation_completed + terminal_status + user_confirmed`），桥接为 `SimArchiveBundle` 契约与 JSON；仍为 `simulate_dry_run`。
- Runtime 新增桥接器（F14）：
  - `FHCIAbilityKitAgentExecutorSimulateExecuteArchiveBundleBridge::BuildSimulateExecuteArchiveBundle(...)`
  - 输入：`SimFinalReport`（F13） + `SimExecuteReceipt`（F12） + `CurrentExecuteTicket`（F11） + `CurrentConfirmRequest`（F10） + `CurrentApplyRequest`（F9） + `CurrentReviewReport`（F6/F8）
  - 输出：`FHCIAbilityKitAgentSimulateExecuteArchiveBundle`
- F14 `SimArchiveBundle` 顶层字段（最小冻结）：
  - `request_id`
  - `sim_final_report_id`
  - `sim_execute_receipt_id`
  - `execute_ticket_id`
  - `confirm_request_id`
  - `apply_request_id`
  - `review_request_id`
  - `selection_digest`
  - `archive_digest`（基于终态摘要与行级关键信息生成的稳定摘要，用于下游存证去重）
  - `generated_utc`（值按北京时间 `+08:00` 输出，字段名兼容保留）
  - `execution_mode`（一期固定：`simulate_dry_run_archive_bundle`）
  - `transaction_mode`（沿用 F11-F13）
  - `termination_policy`（沿用 F11-F13）
  - `terminal_status`（沿用 F13：`completed|blocked`）
  - `archive_status`（`ready|blocked`）
  - `user_confirmed`
  - `ready_to_simulate_execute`
  - `simulated_dispatch_accepted`
  - `simulation_completed`
  - `archive_ready`
  - `error_code`（成功可为 `-`）
  - `reason`
  - `summary`
  - `items[]`
- F14 校验规则（冻结）：
  - `SimFinalReport.user_confirmed=false` -> `archive_ready=false`，`archive_status=blocked`，`error_code=E4005`，`reason=user_not_confirmed`
  - `SimFinalReport.ready_to_simulate_execute=false` -> `archive_ready=false`，`archive_status=blocked`，`error_code=E4205`，`reason=execute_ticket_not_ready`
  - `SimFinalReport.simulated_dispatch_accepted=false` -> `archive_ready=false`，`archive_status=blocked`，`error_code=E4206`，`reason=simulate_execute_receipt_not_accepted`
  - `SimFinalReport.simulation_completed=false` 或 `SimFinalReport.terminal_status!=completed` -> `archive_ready=false`，`archive_status=blocked`，`error_code=E4207`，`reason=simulate_final_report_not_completed`
  - `CurrentExecuteTicket.ready_to_simulate_execute=false` -> `archive_ready=false`，`archive_status=blocked`，`error_code=E4205`，`reason=execute_ticket_not_ready`
  - `CurrentConfirmRequest.ready_to_execute=false` -> `archive_ready=false`，`archive_status=blocked`，`error_code=E4204`，`reason=confirm_request_not_ready`
  - `SimFinalReport.sim_execute_receipt_id != SimExecuteReceipt.request_id` -> `archive_ready=false`，`error_code=E4202`，`reason=sim_execute_receipt_id_mismatch`
  - `SimFinalReport.execute_ticket_id != CurrentExecuteTicket.request_id` -> `archive_ready=false`，`error_code=E4202`，`reason=execute_ticket_id_mismatch`
  - `SimFinalReport.confirm_request_id != CurrentConfirmRequest.request_id` -> `archive_ready=false`，`error_code=E4202`，`reason=confirm_request_id_mismatch`
  - `SimFinalReport.apply_request_id != CurrentApplyRequest.request_id` -> `archive_ready=false`，`error_code=E4202`，`reason=apply_request_id_mismatch`
  - `SimFinalReport.review_request_id != CurrentReviewReport.request_id` -> `archive_ready=false`，`error_code=E4202`，`reason=review_request_id_mismatch`
  - `SimFinalReport.selection_digest` 与 `SimExecuteReceipt/CurrentExecuteTicket/CurrentConfirmRequest/CurrentApplyRequest` 或 `CurrentReviewReport` 重算摘要任一不一致 -> `archive_ready=false`，`error_code=E4202`，`reason=selection_digest_mismatch`
  - 全部通过 -> `archive_ready=true`，`archive_status=ready`，`error_code=-`，`reason=simulate_archive_bundle_ready`
- Editor 新增命令（F14）：
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimArchiveBundle [tamper=none|digest|apply|review|confirm|receipt|final|ready]`
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimArchiveBundleJson [tamper=none|digest|apply|review|confirm|receipt|final|ready]`
  - 输入来源：F13 “最新 SimFinalReport 预览状态” + F12/F11/F10/F9/F6-F8 最新预览状态
  - `tamper` 仅用于 UE 手测模拟：
    - `none`：正常校验
    - `digest`：篡改 `selection_digest`（触发 `E4202/selection_digest_mismatch`）
    - `apply`：篡改 `apply_request_id`（触发 `E4202/apply_request_id_mismatch`）
    - `review`：篡改 `review_request_id`（触发 `E4202/review_request_id_mismatch`）
    - `confirm`：篡改 `confirm_request_id`（触发 `E4202/confirm_request_id_mismatch`）
    - `receipt`：强制 `simulated_dispatch_accepted=false`（触发 `E4206/simulate_execute_receipt_not_accepted`）
    - `final`：强制 `simulation_completed=false` 或 `terminal_status=blocked`（触发 `E4207/simulate_final_report_not_completed`）
    - `ready`：强制 `ready_to_simulate_execute=false`（触发 `E4205/execute_ticket_not_ready`）
- F14 日志口径（新增，冻结）：
  - 摘要（Display/Warning）：
    - `[HCIAbilityKit][AgentExecutorSimArchiveBundle] summary request_id=... sim_final_report_id=... sim_execute_receipt_id=... execute_ticket_id=... confirm_request_id=... apply_request_id=... review_request_id=... selection_digest=... archive_digest=... user_confirmed=true|false ready_to_simulate_execute=true|false simulated_dispatch_accepted=true|false simulation_completed=true|false terminal_status=... archive_status=... archive_ready=true|false error_code=... reason=... validation=ok`
  - 行（Display/Warning）：
    - `row=... tool_name=... risk=... blocked=... skip_reason=... object_type=... locate_strategy=... asset_path=... field=... evidence_key=... actor_path=...`
  - JSON 输出：
    - `[HCIAbilityKit][AgentExecutorSimArchiveBundle] json request_id=... bytes=...`

### 14.5.13 StageF-SliceF15 SimArchiveBundle 最终模拟存证包完整性校验 -> SimHandoffEnvelope（Stage G 输入信封）桥接

- 目标：基于 F14 的 `SimArchiveBundle` 与最新 `SimFinalReport/SimExecuteReceipt/ExecuteTicket/ConfirmRequest/ApplyRequest/Review` 预览执行交接前完整性校验（`sim_archive_bundle_id + sim_final_report_id + sim_execute_receipt_id + execute_ticket_id + confirm_request_id + apply_request_id + review_request_id + selection_digest + archive_digest + archive_ready + archive_status + user_confirmed`），桥接为 `SimHandoffEnvelope` 契约与 JSON；仍为 `simulate_dry_run`。
- Runtime 新增桥接器（F15）：
  - `FHCIAbilityKitAgentExecutorSimulateExecuteHandoffEnvelopeBridge::BuildSimulateExecuteHandoffEnvelope(...)`
  - 输入：`SimArchiveBundle`（F14） + `SimFinalReport`（F13） + `SimExecuteReceipt`（F12） + `CurrentExecuteTicket`（F11） + `CurrentConfirmRequest`（F10） + `CurrentApplyRequest`（F9） + `CurrentReviewReport`（F6/F8）
  - 输出：`FHCIAbilityKitAgentSimulateExecuteHandoffEnvelope`
- F15 `SimHandoffEnvelope` 顶层字段（最小冻结）：
  - `request_id`
  - `sim_archive_bundle_id`
  - `sim_final_report_id`
  - `sim_execute_receipt_id`
  - `execute_ticket_id`
  - `confirm_request_id`
  - `apply_request_id`
  - `review_request_id`
  - `selection_digest`
  - `archive_digest`
  - `handoff_digest`（基于 F15 顶层状态 + 行级关键字段生成的稳定摘要）
  - `generated_utc`（值按北京时间 `+08:00` 输出，字段名兼容保留）
  - `execution_mode`（一期固定：`simulate_dry_run_handoff_envelope`）
  - `handoff_target`（一期固定：`stage_g_execute`）
  - `transaction_mode`
  - `termination_policy`
  - `terminal_status`
  - `archive_status`
  - `handoff_status`（`ready|blocked`）
  - `user_confirmed`
  - `ready_to_simulate_execute`
  - `simulated_dispatch_accepted`
  - `simulation_completed`
  - `archive_ready`
  - `handoff_ready`
  - `error_code`（成功可为 `-`）
  - `reason`
  - `summary`
  - `items[]`
- F15 校验规则（冻结）：
  - `SimArchiveBundle.user_confirmed=false` -> `handoff_ready=false`，`handoff_status=blocked`，`error_code=E4005`，`reason=user_not_confirmed`
  - `SimArchiveBundle.ready_to_simulate_execute=false` -> `handoff_ready=false`，`handoff_status=blocked`，`error_code=E4205`，`reason=execute_ticket_not_ready`
  - `SimArchiveBundle.simulated_dispatch_accepted=false` -> `handoff_ready=false`，`handoff_status=blocked`，`error_code=E4206`，`reason=simulate_execute_receipt_not_accepted`
  - `SimArchiveBundle.simulation_completed=false` 或 `SimArchiveBundle.terminal_status!=completed` -> `handoff_ready=false`，`handoff_status=blocked`，`error_code=E4207`，`reason=simulate_final_report_not_completed`
  - `SimArchiveBundle.archive_ready=false` 或 `SimArchiveBundle.archive_status!=ready` -> `handoff_ready=false`，`handoff_status=blocked`，`error_code=E4208`，`reason=sim_archive_bundle_not_ready`
  - `CurrentExecuteTicket.ready_to_simulate_execute=false` -> `handoff_ready=false`，`error_code=E4205`，`reason=execute_ticket_not_ready`
  - `CurrentConfirmRequest.ready_to_execute=false` -> `handoff_ready=false`，`error_code=E4204`，`reason=confirm_request_not_ready`
  - `SimArchiveBundle.sim_final_report_id != SimFinalReport.request_id` -> `handoff_ready=false`，`error_code=E4202`，`reason=sim_final_report_id_mismatch`
  - `SimArchiveBundle.sim_execute_receipt_id != SimExecuteReceipt.request_id` -> `handoff_ready=false`，`error_code=E4202`，`reason=sim_execute_receipt_id_mismatch`
  - `SimArchiveBundle.execute_ticket_id != CurrentExecuteTicket.request_id` -> `handoff_ready=false`，`error_code=E4202`，`reason=execute_ticket_id_mismatch`
  - `SimArchiveBundle.confirm_request_id != CurrentConfirmRequest.request_id` -> `handoff_ready=false`，`error_code=E4202`，`reason=confirm_request_id_mismatch`
  - `SimArchiveBundle.apply_request_id != CurrentApplyRequest.request_id` -> `handoff_ready=false`，`error_code=E4202`，`reason=apply_request_id_mismatch`
  - `SimArchiveBundle.review_request_id != CurrentReviewReport.request_id` -> `handoff_ready=false`，`error_code=E4202`，`reason=review_request_id_mismatch`
  - `SimArchiveBundle.selection_digest` 与 `SimFinalReport/SimExecuteReceipt/CurrentExecuteTicket/CurrentConfirmRequest/CurrentApplyRequest` 或 `CurrentReviewReport` 重算摘要任一不一致 -> `handoff_ready=false`，`error_code=E4202`，`reason=selection_digest_mismatch`
  - `SimArchiveBundle.archive_digest` 为空 -> `handoff_ready=false`，`error_code=E4202`，`reason=archive_digest_missing`
  - 全部通过 -> `handoff_ready=true`，`handoff_status=ready`，`error_code=-`，`reason=simulate_handoff_envelope_ready`
- Editor 新增命令（F15）：
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimHandoffEnvelope [tamper=none|digest|apply|review|confirm|receipt|final|archive|ready]`
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareSimHandoffEnvelopeJson [tamper=none|digest|apply|review|confirm|receipt|final|archive|ready]`
  - 输入来源：F14 “最新 SimArchiveBundle 预览状态” + F13/F12/F11/F10/F9/F6-F8 最新预览状态
  - `tamper` 仅用于 UE 手测模拟：
    - `none`：正常校验
    - `digest`：篡改 `selection_digest`（触发 `E4202/selection_digest_mismatch`）
    - `apply/review/confirm`：篡改对应 id（触发 `E4202/*_mismatch`）
    - `receipt`：强制 `simulated_dispatch_accepted=false`（触发 `E4206`）
    - `final`：强制 `simulation_completed=false` 或 `terminal_status=blocked`（触发 `E4207`）
    - `archive`：强制 `archive_ready=false` 或 `archive_status=blocked`（触发 `E4208`）
    - `ready`：强制 `ready_to_simulate_execute=false`（触发 `E4205`）
- F15 日志口径（新增，冻结）：
  - 摘要（Display/Warning）：
    - `[HCIAbilityKit][AgentExecutorSimHandoffEnvelope] summary request_id=... sim_archive_bundle_id=... sim_final_report_id=... sim_execute_receipt_id=... execute_ticket_id=... confirm_request_id=... apply_request_id=... review_request_id=... selection_digest=... archive_digest=... handoff_digest=... handoff_target=... user_confirmed=true|false ready_to_simulate_execute=true|false simulated_dispatch_accepted=true|false simulation_completed=true|false terminal_status=... archive_status=... handoff_status=... archive_ready=true|false handoff_ready=true|false error_code=... reason=... validation=ok`
  - 行（Display/Warning）：
    - `row=... tool_name=... risk=... blocked=... skip_reason=... object_type=... locate_strategy=... asset_path=... field=... evidence_key=... actor_path=...`
  - JSON 输出：
    - `[HCIAbilityKit][AgentExecutorSimHandoffEnvelope] json request_id=... bytes=...`

### 14.5.14 StageG-SliceG1 SimHandoffEnvelope（Stage G 输入信封）完整性校验 -> StageGExecuteIntent（Stage G 入口意图包，validate-only）桥接

- 目标：基于 F15 的 `SimHandoffEnvelope` 与最新 `SimArchiveBundle/SimFinalReport/SimExecuteReceipt/ExecuteTicket/ConfirmRequest/ApplyRequest/Review` 预览执行 Stage G 入口准入前完整性校验（`sim_archive_bundle_id + sim_final_report_id + sim_execute_receipt_id + execute_ticket_id + confirm_request_id + apply_request_id + review_request_id + selection_digest + archive_digest + handoff_digest + handoff_ready + handoff_status + user_confirmed`），桥接为 `StageGExecuteIntent` 契约与 JSON；固定 `validate_only` 模式且 `write_enabled=false`，不触发真实资产写入。
- Runtime 新增桥接器（G1）：
  - `FHCIAbilityKitAgentExecutorStageGExecuteIntentBridge::BuildStageGExecuteIntent(...)`
  - 输入：`SimHandoffEnvelope`（F15） + `SimArchiveBundle`（F14） + `SimFinalReport`（F13） + `SimExecuteReceipt`（F12） + `CurrentExecuteTicket`（F11） + `CurrentConfirmRequest`（F10） + `CurrentApplyRequest`（F9） + `CurrentReviewReport`（F6/F8）
  - 输出：`FHCIAbilityKitAgentStageGExecuteIntent`
- G1 `StageGExecuteIntent` 顶层字段（最小冻结）：
  - `request_id`
  - `sim_handoff_envelope_id`
  - `sim_archive_bundle_id`
  - `sim_final_report_id`
  - `sim_execute_receipt_id`
  - `execute_ticket_id`
  - `confirm_request_id`
  - `apply_request_id`
  - `review_request_id`
  - `selection_digest`
  - `archive_digest`
  - `handoff_digest`
  - `execute_intent_digest`
  - `generated_utc`（值按北京时间 `+08:00` 输出，字段名兼容保留）
  - `execution_mode`（一期固定：`stage_g_validate_only`）
  - `execute_target`（一期固定：`stage_g_execute_runtime`）
  - `handoff_target`
  - `transaction_mode`
  - `termination_policy`
  - `terminal_status`
  - `archive_status`
  - `handoff_status`
  - `stage_g_status`（`ready|blocked`）
  - `user_confirmed`
  - `ready_to_simulate_execute`
  - `simulated_dispatch_accepted`
  - `simulation_completed`
  - `archive_ready`
  - `handoff_ready`
  - `write_enabled`（一期固定：`false`）
  - `ready_for_stage_g_entry`
  - `error_code`（成功可为 `-`）
  - `reason`
  - `summary`
  - `items[]`
- G1 校验规则（冻结，最小）：
  - `SimHandoffEnvelope.user_confirmed=false` -> `ready_for_stage_g_entry=false`，`stage_g_status=blocked`，`error_code=E4005`，`reason=user_not_confirmed`
  - `SimHandoffEnvelope.ready_to_simulate_execute=false` -> `ready_for_stage_g_entry=false`，`stage_g_status=blocked`，`error_code=E4205`，`reason=execute_ticket_not_ready`
  - `SimHandoffEnvelope.simulated_dispatch_accepted=false` -> `ready_for_stage_g_entry=false`，`stage_g_status=blocked`，`error_code=E4206`，`reason=simulate_execute_receipt_not_accepted`
  - `SimHandoffEnvelope.simulation_completed=false` 或 `SimHandoffEnvelope.terminal_status!=completed` -> `ready_for_stage_g_entry=false`，`stage_g_status=blocked`，`error_code=E4207`，`reason=simulate_final_report_not_completed`
  - `SimHandoffEnvelope.archive_ready=false` 或 `SimHandoffEnvelope.archive_status!=ready` -> `ready_for_stage_g_entry=false`，`stage_g_status=blocked`，`error_code=E4208`，`reason=sim_archive_bundle_not_ready`
  - `SimHandoffEnvelope.handoff_ready=false` 或 `SimHandoffEnvelope.handoff_status!=ready` -> `ready_for_stage_g_entry=false`，`stage_g_status=blocked`，`error_code=E4209`，`reason=sim_handoff_envelope_not_ready`
  - `CurrentExecuteTicket.ready_to_simulate_execute=false` -> `ready_for_stage_g_entry=false`，`error_code=E4205`，`reason=execute_ticket_not_ready`
  - `CurrentConfirmRequest.ready_to_execute=false` -> `ready_for_stage_g_entry=false`，`error_code=E4204`，`reason=confirm_request_not_ready`
  - `SimHandoffEnvelope.sim_archive_bundle_id != SimArchiveBundle.request_id` -> `ready_for_stage_g_entry=false`，`error_code=E4202`，`reason=sim_archive_bundle_id_mismatch`
  - `SimHandoffEnvelope.sim_final_report_id != SimFinalReport.request_id` -> `ready_for_stage_g_entry=false`，`error_code=E4202`，`reason=sim_final_report_id_mismatch`
  - `SimHandoffEnvelope.sim_execute_receipt_id != SimExecuteReceipt.request_id` -> `ready_for_stage_g_entry=false`，`error_code=E4202`，`reason=sim_execute_receipt_id_mismatch`
  - `SimHandoffEnvelope.execute_ticket_id != CurrentExecuteTicket.request_id` -> `ready_for_stage_g_entry=false`，`error_code=E4202`，`reason=execute_ticket_id_mismatch`
  - `SimHandoffEnvelope.confirm_request_id != CurrentConfirmRequest.request_id` -> `ready_for_stage_g_entry=false`，`error_code=E4202`，`reason=confirm_request_id_mismatch`
  - `SimHandoffEnvelope.apply_request_id != CurrentApplyRequest.request_id` -> `ready_for_stage_g_entry=false`，`error_code=E4202`，`reason=apply_request_id_mismatch`
  - `SimHandoffEnvelope.review_request_id != CurrentReviewReport.request_id` -> `ready_for_stage_g_entry=false`，`error_code=E4202`，`reason=review_request_id_mismatch`
  - `SimHandoffEnvelope.selection_digest` 与 `SimArchiveBundle/SimFinalReport/SimExecuteReceipt/CurrentExecuteTicket/CurrentConfirmRequest/CurrentApplyRequest` 或 `CurrentReviewReport` 重算摘要任一不一致 -> `ready_for_stage_g_entry=false`，`error_code=E4202`，`reason=selection_digest_mismatch`
  - `SimHandoffEnvelope.archive_digest != SimArchiveBundle.archive_digest` 或为空 -> `ready_for_stage_g_entry=false`，`error_code=E4202`，`reason=archive_digest_mismatch_or_missing`
  - `SimHandoffEnvelope.handoff_digest` 为空 -> `ready_for_stage_g_entry=false`，`error_code=E4202`，`reason=handoff_digest_missing`
  - `SimHandoffEnvelope.handoff_target != stage_g_execute` -> `ready_for_stage_g_entry=false`，`error_code=E4202`，`reason=handoff_target_mismatch`
  - 全部通过 -> `ready_for_stage_g_entry=true`，`stage_g_status=ready`，`write_enabled=false`，`error_code=-`，`reason=stage_g_execute_intent_ready_validate_only`
- Editor 新增命令（G1）：
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteIntent [tamper=none|digest|apply|review|confirm|receipt|final|archive|handoff|ready]`
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteIntentJson [tamper=none|digest|apply|review|confirm|receipt|final|archive|handoff|ready]`
  - 输入来源：F15 “最新 SimHandoffEnvelope 预览状态” + F14/F13/F12/F11/F10/F9/F6-F8 最新预览状态
  - `tamper` 仅用于 UE 手测模拟：
    - `none`：正常校验
    - `digest`：篡改 `selection_digest`（触发 `E4202/selection_digest_mismatch`）
    - `apply/review/confirm`：篡改对应 id（触发 `E4202/*_mismatch`）
    - `receipt`：强制 `simulated_dispatch_accepted=false`（触发 `E4206`）
    - `final`：强制 `simulation_completed=false` 或 `terminal_status=blocked`（触发 `E4207`）
    - `archive`：强制 `archive_ready=false` 或 `archive_status=blocked`（触发 `E4208`）
    - `handoff`：强制 `handoff_ready=false` 或 `handoff_status=blocked`（触发 `E4209`）
    - `ready`：强制 `ready_to_simulate_execute=false`（触发 `E4205`）
- G1 日志口径（新增，冻结）：
  - 摘要（Display/Warning）：
    - `[HCIAbilityKit][AgentExecutorStageGExecuteIntent] summary request_id=... sim_handoff_envelope_id=... sim_archive_bundle_id=... sim_final_report_id=... sim_execute_receipt_id=... execute_ticket_id=... confirm_request_id=... apply_request_id=... review_request_id=... selection_digest=... archive_digest=... handoff_digest=... execute_intent_digest=... execute_target=... handoff_target=... user_confirmed=true|false ready_to_simulate_execute=true|false simulated_dispatch_accepted=true|false simulation_completed=true|false terminal_status=... archive_status=... handoff_status=... stage_g_status=... archive_ready=true|false handoff_ready=true|false write_enabled=true|false ready_for_stage_g_entry=true|false error_code=... reason=... validation=ok`
  - 行（Display/Warning）：
    - `row=... tool_name=... risk=... blocked=... skip_reason=... object_type=... locate_strategy=... asset_path=... field=... evidence_key=... actor_path=...`
  - JSON 输出：
    - `[HCIAbilityKit][AgentExecutorStageGExecuteIntent] json request_id=... bytes=...`

### 14.6 一期禁止实现（延期到 Phase 3）

- 记忆门禁与 TTL 细则（Session TTL/SOP 自动提炼触发策略）。
- 线上 KPI 监控体系（时延成本等运营指标）。

### 14.7 LOD 工具安全边界（必须做）

- `SetMeshLODGroup` 执行前必须 `Cast<UStaticMesh>` 成功，否则返回 `E4010`。
- 若目标网格 `NaniteSettings.bEnabled == true`，必须拦截并返回 `E4010`（禁止修改 LODGroup）。
- 仅当目标为 `UStaticMesh` 且 `Nanite` 关闭时，允许执行 LOD 相关写操作。
- Runtime E8 最小判定输出（`FHCIAbilityKitAgentExecutionGate::EvaluateLodToolSafety`）：
  - 输入：`request_id/tool_name/target_object_class/nanite_enabled`
  - 输出：`allowed/error_code/reason/request_id/tool_name/capability/write_like/is_lod_tool/target_object_class/is_static_mesh_target/nanite_enabled`
  - 判定口径（冻结）：
    - `tool_name` 不在 Tool Registry 白名单：返回 `E4002`（`reason=tool_not_whitelisted`）
    - `tool_name != SetMeshLODGroup`：旁路放行（`reason=non_lod_tool_bypass`）
    - `target_object_class` 非 `StaticMesh`（兼容 `UStaticMesh` 归一化）：返回 `E4010`（`reason=lod_tool_requires_static_mesh`）
    - `target_object_class=StaticMesh && nanite_enabled=true`：返回 `E4010`（`reason=lod_tool_nanite_enabled_blocked`）
    - `target_object_class=StaticMesh && nanite_enabled=false`：允许执行（`reason=lod_tool_static_mesh_non_nanite_allowed`）
- Editor UE 手测入口（控制台）：
  - `HCIAbilityKit.AgentLodSafetyDemo`
    - 默认输出三案例：`type_not_static_mesh_blocked / nanite_static_mesh_blocked / static_mesh_non_nanite_allowed`
    - 输出摘要：`summary total_cases=... type_blocked=... nanite_blocked=... expected_blocked_code=E4010 validation=ok`
  - `HCIAbilityKit.AgentLodSafetyDemo [tool_name] [target_object_class] [nanite_enabled 0|1]`
    - 用于单案例验证类型拦截、Nanite 拦截与合法放行日志口径
- 案例日志最小字段（E8）：
  - `request_id/tool_name/capability/write_like/is_lod_tool`
  - `target_object_class/is_static_mesh_target/nanite_enabled`
  - `allowed/error_code/reason`

### 14.8 关卡排雷与命名溯源工具边界（必须做）

- `ScanLevelMeshRisks` 仅允许扫描 `StaticMeshActor`；`scope` 仅允许 `selected/all`，`checks` 仅允许 `missing_collision/default_material`，非法参数返回 `E4011`。
- 关卡风险扫描结果必须带 `actor_path` 与检测证据，并支持定位策略 `camera_focus`。
- `NormalizeAssetNamingByMetadata` 必须先读取 `UAssetImportData/AssetUserData` 再生成提案；无法获取足够元数据时返回 `E4012`，禁止盲改。
- 命名提案必须满足前缀约束：`UStaticMesh -> SM_`，`UTexture* -> T_`；归档目标路径必须以 `/Game/` 开头。
- 命名与归档写操作必须先产出 Dry-Run Diff，未确认不得执行。

### 14.5.15 StageG-SliceG2 StageGExecuteIntent（validate-only 入口意图包）完整性校验 + Stage G 写权限二次确认 -> StageGWriteEnableRequest（写权限启用请求，dry-run）桥接

- 目标：基于 G1 的 `StageGExecuteIntent` 与最新 `SimHandoffEnvelope/SimArchiveBundle/SimFinalReport/SimExecuteReceipt/ExecuteTicket/ConfirmRequest/ApplyRequest/Review` 预览执行完整性校验，并增加 `write_enable_confirmed` 二次确认门禁，桥接为 `StageGWriteEnableRequest` 契约与 JSON；仍为 dry-run，不触发真实资产写入。
- Runtime 桥接接口：`FHCIAbilityKitAgentExecutorStageGWriteEnableRequestBridge::BuildStageGWriteEnableRequest(...)`
- 输出：`FHCIAbilityKitAgentStageGWriteEnableRequest`

- G2 顶层字段（最小冻结）：
  - `request_id/stage_g_execute_intent_id`
  - `sim_handoff_envelope_id/sim_archive_bundle_id/sim_final_report_id/sim_execute_receipt_id`
  - `execute_ticket_id/confirm_request_id/apply_request_id/review_request_id`
  - `selection_digest/archive_digest/handoff_digest/execute_intent_digest/stage_g_write_enable_digest`
  - `execute_target/handoff_target`
  - `terminal_status/archive_status/handoff_status/stage_g_status/stage_g_write_status`
  - `user_confirmed/ready_to_simulate_execute/simulated_dispatch_accepted/simulation_completed/archive_ready/handoff_ready`
  - `write_enabled/ready_for_stage_g_entry/write_enable_confirmed/ready_for_stage_g_execute`
  - `error_code/reason`
  - `summary/items`（保留 `blocked/skip_reason/object_type/locate_strategy/evidence_key/actor_path`）

- 成功口径（G2）：
  - `write_enable_confirmed=true`
  - `ready_for_stage_g_execute=true`
  - `write_enabled=true`
  - `stage_g_write_status=ready`
  - `error_code=-`
  - `reason=stage_g_write_enable_request_ready`

- 错误码与原因（新增/延续）：
  - `E4005 / user_not_confirmed`
  - `E4005 / stage_g_write_enable_not_confirmed`
  - `E4210 / stage_g_execute_intent_not_ready`
  - `E4209 / sim_handoff_envelope_not_ready`
  - `E4208 / sim_archive_bundle_not_ready`
  - `E4207 / simulate_final_report_not_completed`
  - `E4206 / simulate_execute_receipt_not_accepted`
  - `E4205 / execute_ticket_not_ready`
  - `E4204 / confirm_request_not_ready`
  - `E4202 / *_mismatch`

- UE 控制台命令（G2）：
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGWriteEnableRequest [write_enable_confirmed=0|1] [tamper=none|digest|intent|handoff|ready]`
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGWriteEnableRequestJson [write_enable_confirmed=0|1] [tamper=none|digest|intent|handoff|ready]`

### 14.5.16 StageG-SliceG3 StageGWriteEnableRequest（写权限启用请求，dry-run）完整性校验 -> StageGExecutePermitTicket（Stage G 执行许可票据，dry-run）桥接

- 目标：基于 G2 的 `StageGWriteEnableRequest` 与最新 `StageGExecuteIntent/SimHandoffEnvelope/SimArchiveBundle/SimFinalReport/SimExecuteReceipt/ExecuteTicket/ConfirmRequest/ApplyRequest/Review` 预览执行完整性校验，桥接为 `StageGExecutePermitTicket` 契约与 JSON；仍为 dry-run，不触发真实资产写入。
- Runtime 桥接接口：`FHCIAbilityKitAgentExecutorStageGExecutePermitTicketBridge::BuildStageGExecutePermitTicket(...)`
- 输出：`FHCIAbilityKitAgentStageGExecutePermitTicket`

- G3 顶层字段（最小冻结）：
  - `request_id/stage_g_write_enable_request_id/stage_g_execute_intent_id`
  - `sim_handoff_envelope_id/sim_archive_bundle_id/sim_final_report_id/sim_execute_receipt_id`
  - `execute_ticket_id/confirm_request_id/apply_request_id/review_request_id`
  - `selection_digest/archive_digest/handoff_digest/execute_intent_digest/stage_g_write_enable_digest/stage_g_execute_permit_digest`
  - `execute_target/handoff_target`
  - `terminal_status/archive_status/handoff_status/stage_g_status/stage_g_write_status/stage_g_execute_permit_status`
  - `user_confirmed/ready_to_simulate_execute/simulated_dispatch_accepted/simulation_completed/archive_ready/handoff_ready`
  - `write_enabled/ready_for_stage_g_entry/write_enable_confirmed/ready_for_stage_g_execute/stage_g_execute_permit_ready`
  - `error_code/reason`
  - `summary/items`（保留 `blocked/skip_reason/object_type/locate_strategy/evidence_key/actor_path`）

- 成功口径（G3）：
  - `stage_g_execute_permit_ready=true`
  - `write_enabled=true`
  - `ready_for_stage_g_execute=true`
  - `stage_g_execute_permit_status=ready`
  - `error_code=-`
  - `reason=stage_g_execute_permit_ticket_ready`

- 错误码与原因（新增/延续）：
  - `E4211 / stage_g_write_enable_request_not_ready`（新增）
  - `E4210 / stage_g_execute_intent_not_ready`
  - `E4209 / sim_handoff_envelope_not_ready`
  - `E4208 / sim_archive_bundle_not_ready`
  - `E4207 / simulate_final_report_not_completed`
  - `E4206 / simulate_execute_receipt_not_accepted`
  - `E4205 / execute_ticket_not_ready`
  - `E4204 / confirm_request_not_ready`
  - `E4005 / user_not_confirmed`
  - `E4005 / stage_g_write_enable_not_confirmed`
  - `E4202 / *_mismatch`

- UE 控制台命令（G3）：
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecutePermitTicket [write_enable_confirmed=0|1] [tamper=none|digest|intent|handoff|write|ready]`
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecutePermitTicketJson [write_enable_confirmed=0|1] [tamper=none|digest|intent|handoff|write|ready]`

### 14.5.17 StageG-SliceG4 StageGExecutePermitTicket（Stage G 执行许可票据，dry-run）完整性校验 + 执行派发二次确认 -> StageGExecuteDispatchRequest（Stage G 执行派发请求，dry-run）桥接

- 目标：基于 G3 的 `StageGExecutePermitTicket` 与最新 `StageGWriteEnableRequest/StageGExecuteIntent/SimHandoffEnvelope/SimArchiveBundle/SimFinalReport/SimExecuteReceipt/ExecuteTicket/ConfirmRequest/ApplyRequest/Review` 预览执行完整性校验，并引入 `execute_dispatch_confirmed` 二次确认门禁，桥接为 `StageGExecuteDispatchRequest` 契约与 JSON；仍为 dry-run，不触发真实资产写入。
- Runtime 桥接接口：`FHCIAbilityKitAgentExecutorStageGExecuteDispatchRequestBridge::BuildStageGExecuteDispatchRequest(...)`
- 输出：`FHCIAbilityKitAgentStageGExecuteDispatchRequest`

- G4 顶层字段（最小冻结）：
  - `request_id/stage_g_execute_permit_ticket_id/stage_g_write_enable_request_id/stage_g_execute_intent_id`
  - `sim_handoff_envelope_id/sim_archive_bundle_id/sim_final_report_id/sim_execute_receipt_id`
  - `execute_ticket_id/confirm_request_id/apply_request_id/review_request_id`
  - `selection_digest/archive_digest/handoff_digest/execute_intent_digest/stage_g_write_enable_digest/stage_g_execute_permit_digest/stage_g_execute_dispatch_digest`
  - `execute_target/handoff_target`
  - `terminal_status/archive_status/handoff_status/stage_g_status/stage_g_write_status/stage_g_execute_permit_status/stage_g_execute_dispatch_status`
  - `user_confirmed/write_enable_confirmed/execute_dispatch_confirmed`
  - `ready_to_simulate_execute/simulated_dispatch_accepted/simulation_completed/archive_ready/handoff_ready`
  - `write_enabled/ready_for_stage_g_entry/ready_for_stage_g_execute/stage_g_execute_permit_ready/stage_g_execute_dispatch_ready`
  - `error_code/reason`
  - `summary/items`（保留 `blocked/skip_reason/object_type/locate_strategy/evidence_key/actor_path`）

- 成功口径（G4）：
  - `stage_g_execute_dispatch_ready=true`
  - `write_enabled=true`
  - `ready_for_stage_g_execute=true`
  - `stage_g_execute_dispatch_status=ready`
  - `error_code=-`
  - `reason=stage_g_execute_dispatch_request_ready`

- 错误码与原因（新增/延续）：
  - `E4212 / stage_g_execute_permit_ticket_not_ready`（新增）
  - `E4211 / stage_g_write_enable_request_not_ready`
  - `E4210 / stage_g_execute_intent_not_ready`
  - `E4209 / sim_handoff_envelope_not_ready`
  - `E4208 / sim_archive_bundle_not_ready`
  - `E4207 / simulate_final_report_not_completed`
  - `E4206 / simulate_execute_receipt_not_accepted`
  - `E4205 / execute_ticket_not_ready`
  - `E4204 / confirm_request_not_ready`
  - `E4005 / user_not_confirmed`
  - `E4005 / stage_g_write_enable_not_confirmed`
  - `E4005 / stage_g_execute_dispatch_not_confirmed`
  - `E4202 / *_mismatch`

- UE 控制台命令（G4）：
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchRequest [execute_dispatch_confirmed=0|1] [tamper=none|digest|intent|handoff|permit|ready]`
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchRequestJson [execute_dispatch_confirmed=0|1] [tamper=none|digest|intent|handoff|permit|ready]`

### 14.5.18 StageG-SliceG5 StageGExecuteDispatchRequest（Stage G 执行派发请求，dry-run）完整性校验 -> StageGExecuteDispatchReceipt（Stage G 执行派发回执，dry-run）桥接

- 目标：基于 G4 的 `StageGExecuteDispatchRequest` 与最新 `StageGExecutePermitTicket/StageGWriteEnableRequest/StageGExecuteIntent/SimHandoffEnvelope/SimArchiveBundle/SimFinalReport/SimExecuteReceipt/ExecuteTicket/ConfirmRequest/ApplyRequest/Review` 预览执行完整性校验，桥接为 `StageGExecuteDispatchReceipt` 契约与 JSON；仍为 dry-run，不触发真实资产写入。
- Runtime 桥接接口：`FHCIAbilityKitAgentExecutorStageGExecuteDispatchReceiptBridge::BuildStageGExecuteDispatchReceipt(...)`
- 输出：`FHCIAbilityKitAgentStageGExecuteDispatchReceipt`

- G5 顶层字段（最小冻结）：
  - `request_id/stage_g_execute_dispatch_request_id/stage_g_execute_permit_ticket_id/stage_g_write_enable_request_id/stage_g_execute_intent_id`
  - `sim_handoff_envelope_id/sim_archive_bundle_id/sim_final_report_id/sim_execute_receipt_id`
  - `execute_ticket_id/confirm_request_id/apply_request_id/review_request_id`
  - `selection_digest/archive_digest/handoff_digest/execute_intent_digest/stage_g_write_enable_digest/stage_g_execute_permit_digest/stage_g_execute_dispatch_digest/stage_g_execute_dispatch_receipt_digest`
  - `execute_target/handoff_target`
  - `terminal_status/archive_status/handoff_status/stage_g_status/stage_g_write_status/stage_g_execute_permit_status/stage_g_execute_dispatch_status/stage_g_execute_dispatch_receipt_status`
  - `user_confirmed/write_enable_confirmed/execute_dispatch_confirmed`
  - `ready_to_simulate_execute/simulated_dispatch_accepted/simulation_completed/archive_ready/handoff_ready`
  - `write_enabled/ready_for_stage_g_entry/ready_for_stage_g_execute/stage_g_execute_permit_ready/stage_g_execute_dispatch_ready`
  - `stage_g_execute_dispatch_accepted/stage_g_execute_dispatch_receipt_ready`
  - `error_code/reason`
  - `summary/items`（保留 `blocked/skip_reason/object_type/locate_strategy/evidence_key/actor_path`）

- 成功口径（G5）：
  - `stage_g_execute_dispatch_receipt_ready=true`
  - `stage_g_execute_dispatch_accepted=true`
  - `write_enabled=true`
  - `ready_for_stage_g_execute=true`
  - `stage_g_execute_dispatch_receipt_status=accepted`
  - `error_code=-`
  - `reason=stage_g_execute_dispatch_receipt_ready`

- 错误码与原因（新增/延续）：
  - `E4213 / stage_g_execute_dispatch_request_not_ready`（新增）
  - `E4212 / stage_g_execute_permit_ticket_not_ready`
  - `E4211 / stage_g_write_enable_request_not_ready`
  - `E4210 / stage_g_execute_intent_not_ready`
  - `E4209 / sim_handoff_envelope_not_ready`
  - `E4208 / sim_archive_bundle_not_ready`
  - `E4207 / simulate_final_report_not_completed`
  - `E4206 / simulate_execute_receipt_not_accepted`
  - `E4205 / execute_ticket_not_ready`
  - `E4204 / confirm_request_not_ready`
  - `E4005 / user_not_confirmed`
  - `E4005 / stage_g_write_enable_not_confirmed`
  - `E4005 / stage_g_execute_dispatch_not_confirmed`
  - `E4202 / *_mismatch`

- UE 控制台命令（G5）：
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchReceipt [tamper=none|digest|intent|handoff|dispatch|ready]`
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteDispatchReceiptJson [tamper=none|digest|intent|handoff|dispatch|ready]`

### 14.5.19 StageG-SliceG6 StageGExecuteDispatchReceipt（Stage G 执行派发回执，dry-run）完整性校验 + 执行提交二次确认 -> StageGExecuteCommitRequest（Stage G 执行提交请求，dry-run）桥接

- 目标：基于 G5 的 `StageGExecuteDispatchReceipt` 与最新 `StageGExecuteDispatchRequest/StageGExecutePermitTicket/StageGWriteEnableRequest/StageGExecuteIntent/SimHandoffEnvelope/SimArchiveBundle/SimFinalReport/SimExecuteReceipt/ExecuteTicket/ConfirmRequest/ApplyRequest/Review` 预览执行完整性校验，并引入 `execute_commit_confirmed` 二次确认门禁，桥接为 `StageGExecuteCommitRequest` 契约与 JSON；仍为 dry-run，不触发真实资产写入。
- Runtime 桥接接口：`FHCIAbilityKitAgentExecutorStageGExecuteCommitRequestBridge::BuildStageGExecuteCommitRequest(...)`
- 输出：`FHCIAbilityKitAgentStageGExecuteCommitRequest`

- G6 顶层字段（最小冻结）：
  - `request_id/stage_g_execute_dispatch_receipt_id/stage_g_execute_dispatch_request_id/stage_g_execute_permit_ticket_id/stage_g_write_enable_request_id/stage_g_execute_intent_id`
  - `sim_handoff_envelope_id/sim_archive_bundle_id/sim_final_report_id/sim_execute_receipt_id`
  - `execute_ticket_id/confirm_request_id/apply_request_id/review_request_id`
  - `selection_digest/archive_digest/handoff_digest/execute_intent_digest/stage_g_write_enable_digest/stage_g_execute_permit_digest/stage_g_execute_dispatch_digest/stage_g_execute_dispatch_receipt_digest/stage_g_execute_commit_request_digest`
  - `execute_target/handoff_target`
  - `terminal_status/archive_status/handoff_status/stage_g_status/stage_g_write_status/stage_g_execute_permit_status/stage_g_execute_dispatch_status/stage_g_execute_dispatch_receipt_status/stage_g_execute_commit_request_status`
  - `user_confirmed/write_enable_confirmed/execute_dispatch_confirmed/execute_commit_confirmed`
  - `ready_to_simulate_execute/simulated_dispatch_accepted/simulation_completed/archive_ready/handoff_ready`
  - `write_enabled/ready_for_stage_g_entry/ready_for_stage_g_execute/stage_g_execute_permit_ready/stage_g_execute_dispatch_ready`
  - `stage_g_execute_dispatch_accepted/stage_g_execute_dispatch_receipt_ready/stage_g_execute_commit_request_ready`
  - `error_code/reason`
  - `summary/items`（保留 `blocked/skip_reason/object_type/locate_strategy/evidence_key/actor_path`）

- 成功口径（G6）：
  - `stage_g_execute_commit_request_ready=true`
  - `execute_commit_confirmed=true`
  - `write_enabled=true`
  - `ready_for_stage_g_execute=true`
  - `stage_g_execute_commit_request_status=ready`
  - `error_code=-`
  - `reason=stage_g_execute_commit_request_ready`

- 错误码与原因（新增/延续）：
  - `E4214 / stage_g_execute_dispatch_receipt_not_ready`（新增）
  - `E4213 / stage_g_execute_dispatch_request_not_ready`
  - `E4212 / stage_g_execute_permit_ticket_not_ready`
  - `E4211 / stage_g_write_enable_request_not_ready`
  - `E4210 / stage_g_execute_intent_not_ready`
  - `E4209 / sim_handoff_envelope_not_ready`
  - `E4208 / sim_archive_bundle_not_ready`
  - `E4207 / simulate_final_report_not_completed`
  - `E4206 / simulate_execute_receipt_not_accepted`
  - `E4205 / execute_ticket_not_ready`
  - `E4204 / confirm_request_not_ready`
  - `E4005 / user_not_confirmed`
  - `E4005 / stage_g_write_enable_not_confirmed`
  - `E4005 / stage_g_execute_dispatch_not_confirmed`
  - `E4005 / stage_g_execute_commit_not_confirmed`
  - `E4202 / *_mismatch`

- UE 控制台命令（G6）：
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteCommitRequest [execute_commit_confirmed=0|1] [tamper=none|digest|intent|handoff|dispatch|receipt|ready]`
  - `HCIAbilityKit.AgentExecutePlanReviewPrepareStageGExecuteCommitRequestJson [execute_commit_confirmed=0|1] [tamper=none|digest|intent|handoff|dispatch|receipt|ready]`
