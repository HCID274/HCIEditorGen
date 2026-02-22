# HCIAbilityKit Collaboration Contract (V2)

Scope: whole repo.

## 1. 产品定位与目标

- 聚焦单一模块：`HCIAbilityKit`。
- 解决三个核心痛点：资产规范执行难、性能风险发现慢、数据校验累。
- 核心理念：工具能力服务于编辑器效率与数据质量。
- 功能规划必须对齐真实开发场景（优先资产审计与跨部门协同），禁止仅为演示而做“玩具化功能”。
- 当前交付收束：仅做“资产审计主链路”专精模块；不做无确认自动改写资产与审批/审阅 UI。
- 路线衔接：`B4->D3` 收尾后，进入 `Stage E（Agent安全执行）` 与 `Stage F（指令解析与编排）`。
- 增强项（储备池）允许文档保留，但不得阻塞当前切片门禁。
- 一期冻结决议（2026-02-21）：
  - 深做：`Plan JSON 契约`、`Tool Registry 能力声明`、`Dry-Run Diff + Camera Focus`，并强制覆盖三维业务闭环：`资产规范合规（面数阈值+自动LOD、Texture NPOT+分辨率降级）`、`关卡场景排雷（StaticMeshActor 缺失 Collision/Default Material 检测与定位）`、`命名溯源整理（基于 UAssetImportData/AssetUserData 的自动前缀命名与批量 Move 归档）`。
  - 极简：`MAX_ASSET_MODIFY_LIMIT=50`、`All-or-Nothing`、`SourceControl Fail-Fast`、本地 Mock 鉴权与日志。
  - 延期：记忆门禁/TTL 与线上 KPI（Phase 3，不进入一期代码）。
  - 细则：`args_schema` 必须冻结枚举边界；Actor 用 `Camera Focus`，纯资产用 `SyncBrowserToObjects`；SourceControl 未启用时进入离线本地模式；未命中用户默认 `Guest(read_only)`；LOD 工具必须拦截 Nanite 资产。

## 2. 架构基线（冻结）

- 固定采用插件双模块：
  - `HCIAbilityKitRuntime`：数据定义、解析与校验服务。
  - `HCIAbilityKitEditor`：Factory/Reimport、菜单与交互工具。
- 依赖方向必须单向：`Editor -> Runtime`。
- 设计原则：接口简洁、实现深入、依赖抽象、高内聚低耦合。

## 3. 智能中介者工作流（冻结）

- 采用三段式：`计划 -> 执行 -> 审阅`。
- A 层（语义路由）：`Skill(.md + .py)` 轻量调度。
- B 层（执行审计）：生成者与审计者串行复核。
- C 层（交互审阅）：Diff 风格逐项采纳，非黑盒覆盖。
- 敏感操作（修改/删除/批量写入）必须先给出原因并走授权门禁。
- Prompt/Skill 边界：一次性低风险分析可直接 Prompt；可复现、可审计、固定流程能力必须走 Skill。

## 4. 开发节奏与门禁

- 每个切片开发完成后必须停下，等待用户在 UE 手测。
- 用户未明确 Pass，不得进入下一个切片。
- 重构优先级：先结构迁移，再服务抽离，再审计主链路完善，再 UI 升级。

## 5. 测试与数据留存

- Source of Truth：`SourceData/AbilityKits/`。
- 测试记录：`Source/HCIEditorGen/文档/测试记录/`。
- 每次测试必须记录：目标、步骤、预期、结果、证据、结论。

## 6. 文档治理

- 进度唯一权威：`Source/HCIEditorGen/文档/00_总进度.md`。
- 方案唯一权威：`Source/HCIEditorGen/文档/01_技术方案_插件双模块.md`。
- 执行主计划权威：`Source/HCIEditorGen/文档/05_开发执行总方案_资产审计.md`（当前主线切片必须按此顺序推进）。
- 代码结构发生变化（新增/删除/移动关键代码文件）时，必须同步更新：`Source/HCIEditorGen/文档/04_代码框架总览_树状图.md`（树内中文注释需与实际一致）。
- 任何范围/里程碑变化，先改文档再改代码。
- 保持文档轻量，不扩散到无关主题。

## 7. 质量红线

- Runtime 禁止依赖 Editor-only 模块。
- 失败路径禁止脏写和半覆盖资产。
- 错误信息必须可定位（文件/字段/原因/建议）。
- 未通过验证的结论不得标记为“完成”。

## 8. 当前进度快照（2026-02-22）

- Step 1（结构迁移）已关闭：
  - Slice1：插件双模块骨架落地并通过。
  - Slice2：资产/Factory/Reimport 迁移到插件并通过。
  - Slice3：项目本体 Editor 空壳模块下线并通过。
  - Slice4：Reimport 失败通知可视化（含错误原因）并通过。
- Step 2（服务抽离）已关闭：
  - Slice1：JSON 解析从 `UHCIAbilityKitFactory` 抽离到 Runtime `FHCIAbilityKitParserService`，通过。
  - Slice2：打通 `Factory -> Runtime Service -> UE Python` 最小链路，受控失败可见（`__force_python_fail__`），通过。
  - Slice3：Python 结构化输出接入 `DisplayName` 字段映射，通过。
  - Slice4：统一错误契约与 Python 审计输出，通过。
- 检索实验线（历史基线）：
  - `Step3-Slice1~Slice3` 代码与门禁已完成，保留为历史基线能力。
- 当前主线（资产审计优先）：
  - `Stage A-SliceA1` 已完成：路线重规划文档冻结。
  - `Stage A-SliceA2` 已完成：审计结果契约/面数提取/RuleRegistry 冻结。
  - `Stage A-SliceA3` 已完成：实战资产导入方案冻结（种子资产、RepresentingMesh 引用链、Stage B 启动门禁）。
  - `Stage B-SliceB0` 已通过：种子资产清单扫描与 10k 引用链绑定验证。
  - B0 结果：
    - 已完成：Seed 资产体检（`/Game/Seed` 资产类型分布、StaticMesh 面数读取）。
    - 已完成：在用户授权下批量重命名 12 个 StaticMesh，命名规范化为 `SM_*`（不合规从 12 降至 0）。
    - 已完成：`seed_mesh_manifest` build/validate（12 条）与 10k 生成绑定验证（`representing_mesh_assigned=10000`）。
    - 已完成：UE 手测门禁，用户已反馈 `StageB-SliceB0 Pass`。
  - `Stage B-SliceB1` 已通过：`RepresentingMesh` 字段接入与导入绑定。
  - B1 最新状态：
    - 已完成：`FHCIAbilityKitParsedData` 增加 `RepresentingMeshPath`，并解析 `representing_mesh`（可选）字段。
    - 已完成：`UHCIAbilityKitAsset` 增加 `RepresentingMesh(TSoftObjectPtr<UStaticMesh>)`。
    - 已完成：Factory 增加 `E1006` 校验（路径格式/目标存在/类型为 StaticMesh）并完成 Import/Reimport 绑定。
    - 已完成：Python Hook patch 支持可选 `representing_mesh` 覆盖。
    - 已完成：UE 编译通过（`Build.bat HCIEditorGenEditor Win64 Development ...`）。
    - 已完成：UE 导入/重导手测门禁，用户已反馈 `StageB-SliceB1 Pass`。
  - `Stage B-SliceB2` 已通过：`AssetRegistry + FAssetData` 全量枚举。
  - B2 最新状态：
    - 已完成：Runtime 新增 `FHCIAbilityKitAuditScanService`，全量枚举 `UHCIAbilityKitAsset` 元数据（不加载资产对象）。
    - 已完成：`UHCIAbilityKitAsset::GetAssetRegistryTags` 写入 `hci_id/hci_display_name/hci_damage/hci_representing_mesh` 标签，支撑纯元数据扫描。
    - 已完成：Editor 新增控制台命令 `HCIAbilityKit.AuditScan [log_top_n]`，输出扫描统计与样本行。
    - 已完成：UE 编译通过（`Build.bat HCIEditorGenEditor Win64 Development ...`）。
    - 已完成：UE 手测通过（`assets=2`，其中 1 个历史资产字段为空导致覆盖率 `50%`，符合预期；用户反馈 `Pass`）。
  - `Stage B-SliceB4` 已通过：任务中断/重试与失败收敛。
  - B4 最新状态：
    - 已完成：异步扫描状态机抽离（支持 `Running/Cancelled/Failed`）。
    - 已完成：新增控制台命令 `HCIAbilityKit.AuditScanAsyncStop` 与 `HCIAbilityKit.AuditScanAsyncRetry`。
    - 已完成：`HCIAbilityKit.AuditScanProgress` 增强，支持 `cancelled/failed/idle/running` 状态输出。
    - 已完成：UE 手测通过（用户日志命中 `interrupted ... can_retry=true` 与 `retry start ...`，反馈 `OK`）。
  - `Stage B-SliceB5` 已通过：`triangle_count` Tag 采集与 `triangle_source=tag_cached` 来源标记。
  - B5 最新状态：
    - 已完成：扩展 `triangle_count` Tag 候选解析（含 `hci_triangles_lod0/triangle_count_lod0/triangles_lod0/lod0_triangles/Triangles/NumTriangles`）。
    - 已完成：`AuditScan/AuditScanAsync` 输出 `triangle_count_lod0/triangle_source/triangle_source_tag`。
    - 已完成：统计摘要新增 `triangle_tag_coverage`。
    - 已完成：UE 手测通过（用户日志命中 `triangle_tag_coverage=99.5%`，且多条 `triangle_source=tag_cached triangle_source_tag=Triangles`）。
  - `Stage B-SliceB6` 已通过：状态过滤（`Locked/Dirty`）与跳过留痕。
  - B6 最新状态：
    - 已完成：同步/异步扫描统一输出 `scan_state/skip_reason`。
    - 已完成：`Dirty` 包跳过并记录 `scan_state=skipped_locked_or_dirty skip_reason=package_dirty`。
    - 已完成：只读包跳过并记录 `scan_state=skipped_locked_or_dirty skip_reason=package_read_only`。
    - 已完成：摘要新增 `skipped_locked_or_dirty` 计数。
    - 已完成：用户手测确认 `Pass`。
  - `Stage B-SliceB7` 已通过：数据驱动预览体与拖入场景可视化验证。
  - B7 最新状态：
    - 已完成：新增 `AHCIAbilityKitPreviewActor`，读取 `AbilityAsset.RepresentingMesh` 并驱动 `PreviewMeshComponent` 显示。
    - 已完成：支持 `OnConstruction` 与 `RefreshPreview(CallInEditor)` 刷新。
    - 已完成：修复 Details 分类混淆，将分类统一为 `HCIAudit`（去除 `HCI|Audit` 层级分组）。
    - 已完成：用户手测确认 `Pass`。
  - `Stage B-SliceB8` 已通过：预览体自动同步（`PostEditChangeProperty` + Reimport 后主动刷新）。
  - `Stage C-SliceC1` 已通过：RuleRegistry 框架与规则接口落地。
    - 已完成：Runtime 新增 `IHCIAbilityAuditRule` 契约与 `FHCIAbilityKitAuditRuleRegistry` 注册中心。
    - 已完成：默认规则 `TriangleExpectedMismatchRule`（`actual vs expected`）接入扫描链路。
    - 已完成：导入解析支持可选 `params.triangle_count_lod0`，并写入资产标签 `hci_triangle_expected_lod0`。
    - 已完成：`AuditScan/AuditScanAsync` 增加规则 issue 统计字段（`issue_assets/info/warn/error`）与行级 issue 输出。
    - 已完成：自动化测试 `HCIAbilityKit.Editor.AuditRules.TriangleExpectedMismatchWarn` 与 `...TriangleExpectedMissingShouldNotWarn`。
    - 已完成：用户 UE 手测通过（同步/异步扫描无异常，C1 新增摘要/行字段完整且 expected 缺失时不误报）。
  - `Stage C-SliceC2` 已通过：首批规则 `TextureNPOTRule + HighPolyAutoLODRule` 落地。
    - 已完成：默认 RuleRegistry 扩展首批规则 `TextureNPOTRule + HighPolyAutoLODRule`。
    - 已完成：`HighPolyAutoLODRule` 使用 UE 真实 Tag（`Triangles/LODs/NaniteEnabled`）判断高面数且缺失额外 LOD。
    - 已完成：`TextureNPOTRule` 使用 `Dimensions`（`WxH`）判定 NPOT，规则级自动化测试通过。
    - 已完成：`AuditScan/AuditScanAsync` 行日志新增 `class/mesh_lods/mesh_nanite/tex_dims` 字段，便于 UE 手测定位规则证据。
    - 已完成：用户 UE 手测通过（3 个高面数非 Nanite 单 LOD 样本命中 `HighPolyAutoLODRule`；摘要与行日志一致）。
  - `Stage C-SliceC3` 已通过：统一审计结果结构与严重级别。
    - 已完成：Runtime 新增 `FHCIAbilityKitAuditReport / FHCIAbilityKitAuditResultEntry` 统一结果结构。
    - 已完成：`FHCIAbilityKitAuditReportBuilder` 支持将 `ScanSnapshot` 扁平化为 `results[]`（为 `C4` JSON 导出复用）。
    - 已完成：严重级别统一字符串化 `Info/Warn/Error`，扫描行日志新增 `first_issue_severity_name`。
    - 已完成：用户 UE 手测通过（同步/异步扫描正常；`1 <-> Warn` 映射一致）。
  - `Stage C-SliceC4` 已通过：JSON 报告导出（含 `triangle_source` 与规则证据）。
    - 已完成：Runtime 新增 `FHCIAbilityKitAuditReportJsonSerializer`（`AuditReport -> JSON`）。
    - 已完成：Editor 新增命令 `HCIAbilityKit.AuditExportJson [output_json_path]`。
    - 已完成：本地 smoke 验证导出成功并落盘（`c4_local_smoke.json`，含 `triangle_source/evidence`）。
    - 已完成：用户 UE 手测通过（无参数/指定路径导出均成功；日志含 `path/run_id/results`）。
  - `Stage D-SliceD1` 已通过：深度检查批次策略与分批加载释放。
    - 已完成：`HCIAbilityKit.AuditScanAsync` 新增可选参数 `deep_mesh_check=0|1`（默认 `0`）。
    - 已完成：开启深度模式后，按异步批次对 `RepresentingMesh` 执行同步加载，补齐 `triangle_count_lod0/mesh_lods/mesh_nanite` 缺失信号。
    - 已完成：每次加载后立即释放 `FStreamableHandle` 强引用；完成日志输出 `[AuditScanAsync][Deep]` 聚合统计（`load_attempts/load_success/handle_releases/...`）。
    - 已完成：自动化测试新增 `HCIAbilityKit.Editor.AuditScanAsync.DeepModeRetryPersistence`（中断/重试保留深度模式开关），本地通过。
    - 已完成：用户 UE 手测通过（`deep_mesh_check=true` 在 start/retry 保留；深度统计行输出；样本全 `tag_cached` 时统计为 `0` 符合预期）。
  - `Stage D-SliceD2` 已通过：GC 节流策略。
    - 已完成：`HCIAbilityKit.AuditScanAsync` 新增第 4 参数 `gc_every_n_batches>=0`（深度模式默认 `16`）。
    - 已完成：按批次节流调用 `CollectGarbage`（在深度批次处理且释放加载句柄之后执行）。
    - 已完成：深度统计扩展 `batches/gc_every_n_batches/gc_runs/max_batch_ms/max_gc_ms/peak_used_physical_mib`。
    - 已完成：自动化测试新增 `HCIAbilityKit.Editor.AuditScanAsync.GcThrottleRetryPersistence`（重试保留 GC 节流配置），本地通过。
    - 已完成：用户 UE 手测通过（`gc_every_n_batches=1` 高频 GC 命中 `gc_runs=274`；`Stop/Retry` 保留 `gc_every_n_batches=3` 并生效）。
  - `Stage D-SliceD3` 已通过：峰值内存与吞吐日志监控。
    - 已完成：新增运行中性能监控日志 `[HCIAbilityKit][AuditScanAsync][Perf]`，在进度点输出 `avg_assets_per_sec/window_assets_per_sec/eta_s/used_physical_mib/peak_used_physical_mib/gc_runs`。
    - 已完成：新增完成态性能汇总日志 `[HCIAbilityKit][AuditScanAsync][PerfSummary]`，输出 `avg/p50/p95/max_batch_assets_per_sec` 与峰值内存。
    - 已完成：新增性能统计辅助 `FHCIAbilityKitAuditPerfMetrics`（吞吐率换算 + nearest-rank 分位数）。
    - 已完成：自动化测试新增 `HCIAbilityKit.Editor.AuditScanAsync.PerfMetricsHelper`，本地通过（`AuditScanAsync` 5/5，`AuditResults` 回归 3/3）。
    - 已完成：用户 UE 手测通过（`AuditScanAsync 1 20 1 3` 无 Error；运行中 `[Perf]` 日志字段完整；完成态 `[PerfSummary]` 输出 `p50/p95/max batch throughput + peak_used_physical_mib`；原有 `[Deep]` 日志仍存在，无回归）。
  - 当前切片：`Stage E-SliceE1`（Tool Registry 能力声明冻结）。
  - 下一切片：`Stage E-SliceE2`（Dry-Run Diff 契约与 UE 面板展示）。
  - D 段收尾后续主线：`Stage E`（安全执行：Dry-Run/Confirm/Transaction/SC）-> `Stage F`（NL->Plan->Executor）。
  - B3 最新状态：
    - 已完成：新增 `HCIAbilityKit.AuditScanAsync [batch_size] [log_top_n]`，按分片执行 `AssetRegistry + FAssetData` 扫描，避免单帧全量阻塞。
    - 已完成：新增 `HCIAbilityKit.AuditScanProgress`，可在扫描期间查询进度。
    - 已完成：异步扫描输出 `progress=%` 与完成摘要（覆盖率/耗时/样本行）。
    - 已完成：UE 编译通过（`Build.bat HCIEditorGenEditor Win64 Development ...`）。
    - 已完成：UE 手测通过（`progress=idle -> start -> 50% -> 100% -> summary -> progress=idle`，用户反馈 `Pass`）。

## 9. 用户协作习惯（固定）

- 优先在聊天中直接给“下一步操作清单”，不要让用户先翻文档再执行。
- 每个切片必须小步推进；完成后立即停下，等待用户在 UE 手测。
- UE 内手工操作由用户执行；代码改动、构建验证、文档回写由助手执行。
- 用户用 `Pass/Fail` 作为门禁信号；未收到 Pass 不得进入下一切片。
- 回归步骤要具体可执行（短步骤、可复现、可定位失败原因）。
- 继续坚持“接口简单、深度功能、依赖抽象、高内聚低耦合”。
