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
