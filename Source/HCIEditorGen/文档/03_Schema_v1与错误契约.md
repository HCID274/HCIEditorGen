# HCIAbilityKit Schema v1 与错误契约

> 状态：冻结（M0-V3）
> 版本：v3
> 更新时间：2026-02-15

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

## 6. 测试样例（固定）

- `TC_OK_01`：合法导入与重导
- `TC_FAIL_01`：删除 `id`（期望 `E1002`）
- `TC_FAIL_02`：`damage: "abc"`（期望 `E1003`）
- `TC_FAIL_03`：源文件失效（期望 `E1005`）
- `TC_FAIL_04`：敏感操作拒绝授权（期望 `E2002`）
- `TC_FAIL_05`：`representing_mesh` 路径不存在（期望 `E1006`）

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
  2. `batch_loaded`：Tag 缺失时，在 Stage D 分批加载 `RepresentingMesh` 对应 `UStaticMesh` 并提取 `LOD0` 三角面数（`GetNumTriangles(0)`）。
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
  - `triangle_source`（`tag_cached/batch_loaded/unavailable`）
  - `lod_index`（默认 `0`）
  - `source_tag_key`（当来源为 Tag 时必填）
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
  - `triangle_source`（`tag_cached/batch_loaded/unavailable`）
  - `virtual_path`（可缺省）
  - `scan_state`（`ok/skipped_locked_or_dirty/skipped_unavailable`）

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
