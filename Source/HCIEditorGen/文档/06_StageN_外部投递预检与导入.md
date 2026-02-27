# Stage N 外部投递（DCC/AI）到 UE 的预检与导入桥接（冻结）

> 状态：冻结（Stage N-SliceN1）  
> 更新时间：2026-02-27  
> 适用：`HCIAbilityKit`（增强项，不改变既有 9 工具白名单与执行语义）

## 1. 目的（第一性原理）

在二游/大世界的高频迭代里，“出问题”的往往不是 UE 里的工具能力，而是**外部资产进入工程的第一步**：

- DCC/AI 资产非标：命名乱、格式不一致、文件损坏、体积异常。
- 直接导入 UE 的风险高：导入过程可能卡死甚至崩溃，且问题难复现、难追责。
- 即使导入成功，也会把“脏数据”带进工程，后续整改成本爆炸。

因此 Stage N 的核心是：把“导入”变成一个**可预检、可追溯、可自动化衔接 Agent 审计链路**的入口。

## 2. 总体设计（N1）

### 2.1 外部侧（Python 投递器）

- 输入：任意来源导出的文件/目录（例如 FBX、PNG、TGA 等）。
- 输出：
  - 批次目录：`Saved/HCIAbilityKit/Ingest/<batch_id>/`
  - 清单文件：`Saved/HCIAbilityKit/Ingest/<batch_id>/manifest.hci.json`
  - 复制后的 staged 文件（保持相对路径）。
- 目标：在 UE 之外先做“挡雷”：
  - 白名单扩展名过滤
  - 基础文件头签名校验（避免明显坏文件进入 UE）
  - 单文件体积阈值拦截
  - 生成结构化 metadata（便于 Agent 之后做灰度决策）

### 2.2 UE 侧（Ingest 发现 + 上下文注入 + 导入命令）

- Ingest 发现：监听/扫描最新 `manifest.hci.json`，缓存为“最近批次”。
- 上下文注入：将“最近批次摘要”注入 Planner 的 `ENV_CONTEXT`，减少对话漂移与上下文工程。
- 规则注入：读取 `Saved/HCIAbilityKit/Rules/Project_Rules.md`（可选），以 `project_rules_md` 形式注入 `ENV_CONTEXT`（最大注入 `12000` 字符，超出截断）。
- Dump 命令：提供只读诊断入口 `HCIAbilityKit.IngestDumpLatest`，快速确认“最近批次”与预检结果。
- 导入命令：提供确定性入口 `HCIAbilityKit.IngestImportLatest`，把 staged 文件导入到 `suggested_unreal_target_root`。

> 说明：N1 不引入新的 Planner 工具；导入属于“确定性工程动作”，由命令触发。导入后再由 Agent 复用既有工具闭环（命名/归档/LOD/贴图等）。

## 3. manifest.hci.json 协议（最小字段集）

示例（字段冻结；允许将来扩展新字段，但不得破坏既有字段含义）：

```json
{
  "schema_version": 1,
  "batch_id": "20260227_153012_box",
  "source_app": "blender",
  "staged_at_utc": "2026-02-27T15:30:12Z",
  "suggested_unreal_target_root": "/Game/Temp/Ingest/20260227_153012_box",
  "files": [
    {
      "relative_path": "Meshes/untitled_box_final_v2.fbx",
      "kind": "mesh",
      "size_bytes": 1234567,
      "sha1": "0123abcd...",
      "suggested_asset_name": "SM_Box_Final_V2"
    }
  ],
  "preflight": {
    "ok": true,
    "warnings": [],
    "errors": []
  }
}
```

字段说明：

- `suggested_unreal_target_root` 必须以 `/Game/` 开头；UE 侧会再次校验，不合法直接拒绝导入。
- `relative_path` 必须是批次目录内相对路径（不允许 `..`）。
- `preflight.ok=false` 时 UE 侧默认拒绝导入（除非将来专门加“强制导入”开关，N1 不做）。

## 4. 外部预检规则（N1）

- 扩展名白名单：`.fbx .png .tga .jpg .jpeg`
- 头签名检查：
  - PNG：`89 50 4E 47 0D 0A 1A 0A`
  - JPG：`FF D8` 开头
  - FBX：包含 `Kaydara FBX`（ASCII）或二进制头标识
  - TGA：仅做最小长度检查（不做完整解析）
- 体积阈值：默认单文件 `<= 512MB`

## 5. UE 侧导入策略（N1）

- 使用 `UAssetImportTask` / `AssetTools` 执行批量导入。
- 默认导入目录：`manifest.suggested_unreal_target_root`
- 默认不覆盖已有资产（避免误伤；后续如需覆盖要走写门禁方案讨论）。
- 导入前弹出确认（符合“不做无确认写工程”原则）。

## 6. 面试演示脚本（建议）

1. 外部：运行投递器生成批次与清单（录屏）。
2. UE：执行 `HCIAbilityKit.IngestDumpLatest`，确认最近批次与 `preflight_ok=true`（录屏）。
3. UE：执行 `HCIAbilityKit.IngestImportLatest`，确认导入（录屏）。
4. UE：打开 `HCIAbilityKit.AgentChatUI`，输入“帮我规范化刚导入的批次，并归档到 /Game/Art/Organized”。
5. 展示：Agent 计划卡（含写审批卡），你点“通过”后执行、定位、输出摘要。

关键呈现点：
- 审批卡中每个关键写步骤必须展示 `ui_presentation.intent_reason`（为什么这么做 / 为什么豁免）。
- 高风险步骤展示 `ui_presentation.risk_warning`。

## 7. Slice N2：目录分发（Asset Routing）与多级归档（冻结）

> N1 解决“安全导入 + 上下文注入”。N2 解决“导入后如何在工程内长期可维护地存放”：命名只是第一步，目录语义（模块/类型/资产组/子目录）才是大项目里最难维护的部分。

### 7.1 第一性原理

- 人是靠“路径语义”找资产：几千个资产堆在一个目录，团队协作必然崩溃。
- 引擎与版本控制也依赖路径语义：Pak 组织、权限、分支策略、内容审计都与目录结构强相关。
- 因此，“命名规范化工具”必须升级为“命名 + 目录分发器（Asset Router）”。

### 7.2 设计（不新增工具，不改 Plan JSON）

- 仍复用既有 Planner 工具链：`ScanAssets -> NormalizeAssetNamingByMetadata`。
- `NormalizeAssetNamingByMetadata` 的内部策略升级：
  - 在 `target_root` 下，按“资产组（Asset Group）”建立专属文件夹；
  - 按资产类别进入子目录：`Textures/Materials/`（可配置）。
- 写操作仍走审批卡确认（HITL），符合“不做无确认写工程”原则。

### 7.3 路由规则配置（Project_Rules.md 内可选 JSON 代码块）

文件：`Saved/HCIAbilityKit/Rules/Project_Rules.md`

推荐写法：在 Markdown 里嵌入一个结构化代码块（非程序员也能改）：

```text
## Asset Routing

```hci_routing_json
{
  "per_asset_folder": true,
  "group_name_style": "pascal_case",
  "subfolders": {
    "SM": "",
    "T": "Textures",
    "M": "Materials",
    "MI": "Materials"
  }
}
```
```

说明：
- `per_asset_folder=true`：每个资产组独占一个文件夹（推荐，工业常态）。
- `group_name_style`：资产组文件夹命名风格（默认 `pascal_case`，便于阅读）。
- `subfolders`：按前缀（由资产 Class 推导，如 `SM/T/M/MI`）路由到子目录；空字符串表示放在资产组根目录。

若未提供该代码块：系统按默认规则执行（每资产组文件夹 + `Textures/Materials/`）。

### 7.4 UE 手测脚本（N2）

1. 运行投递器：
   - `python SourceData/AbilityKits/Python/hci_ingest_stage.py --input <你的测试资产目录> --source-app manual --suggest-target-root /Game/__HCI_Test/Incoming`
2. UE：`HCIAbilityKit.IngestImportLatest`
3. UE：`HCIAbilityKit.AgentChatUI`
4. 输入：
   - `帮我规范化刚导入的批次，并归档到 /Game/__HCI_Test/Organized`
5. 审批卡：
   - 预期能看到“多级目录分发”的提案（至少 Texture2D 进入 `Textures/` 子目录）
6. 点击 `通过`：
   - 预期资产真实移动、重命名、并 FixUp Redirectors，不出现引用断裂。

## 8. Slice N3：混乱目录重整（依赖感知分组 + Shared 落点 + 可重复回归夹具）

> N2 解决“刚导入批次”的组织问题，但真实项目里经常遇到“目录天然就乱了、历史遗留交错引用”的情况。N3 的目标是让 Asset Router 能在真实混乱目录下工作，并提供可重复回归的测试夹具（每次 Reset 还原混乱态）。

### 8.1 第一性原理

- “目录整理”不是字符串替换，而是**重排依赖图**：`StaticMesh -> Material(Instance) -> Texture`。
- 资产会交叉引用，强行归到某一个组会造成冲突与误伤，因此需要 `Shared/` 落点。
- 回归测试必须可重复：不能整理一次就没法再测，因此需要“一键恢复混乱态”。

### 8.2 夹具命令（只影响 __HCI_Test，Seed 只读）

1. `HCIAbilityKit.SeedChaosBuildSnapshot`
   - 从 `/Game/Seed`（只读）挑选并复制 `<= 50` 个资产到：
     - `/Game/__HCI_Test/Fixtures/SeedChaosSnapshot`
2. `HCIAbilityKit.SeedChaosReset`
   - 弹确认框后，仅删除并重建：
     - `/Game/__HCI_Test/Incoming/SeedChaos`
     - `/Game/__HCI_Test/Organized/SeedClean`
   - 把 Snapshot 复制回 Incoming，并执行：
     - 目录打散（分散到多个子目录）
     - 名字变脏（例如 `SM_xxxxxxxx_final_v2`）
   - 生成报告：
     - `Saved/HCIAbilityKit/TestFixtures/SeedChaos/latest.json`

### 8.3 路由策略（不新增 Planner 工具）

- Planner 工具链保持不变：`ScanAssets -> NormalizeAssetNamingByMetadata`
- `NormalizeAssetNamingByMetadata` 内部升级为依赖感知：
  - 以 `StaticMesh/SkeletalMesh` 作为 Anchor（资产组锚点）
  - 使用 `AssetRegistry` 依赖（Hard Dependencies）做传递归并
  - 若某资源被多个 Anchor 引用，落到 `target_root/Shared/...`

### 8.4 UE 手测脚本（N3）

1. 首次生成快照：
   - `HCIAbilityKit.SeedChaosBuildSnapshot`
2. 每次回归重置：
   - `HCIAbilityKit.SeedChaosReset`
3. Agent 整理：
   - `HCIAbilityKit.AgentChatUI`
   - 输入：
     - `扫描 /Game/__HCI_Test/Incoming/SeedChaos，然后按元数据规范命名并移动归档到 /Game/__HCI_Test/Organized/SeedClean。`
4. 审批卡点击 `通过`：
   - 预期能看到多个目标目录（含 `Shared/`），执行后结构稳定且不丢引用。
