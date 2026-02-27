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
