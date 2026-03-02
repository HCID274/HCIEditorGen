# HCIEditorGen（UE5 工具链Demo）— HCIAbilityKit

> 基于 LLM Agent 的 UE 智能资产管线与审计系统
> 关键词：UE 编辑器工具 / Technical Art / 资产合规审计 / AI 辅助规划 / Dry-Run Diff / 安全执行门禁 / 批量处理


视频8s展示：文件整理，规范命名，自动文件夹归类整理
https://github.com/user-attachments/assets/de2a4f6a-587f-4958-b7b5-852f302a29b3


视频8s展示：按照文件名自动挂载资产Link
https://github.com/user-attachments/assets/59471a75-4778-4b51-b356-8a466d4fa9d4



---

## 这个项目解决什么问题？

面向美术/TA 的编辑器内工具化管线：把“**大批量资产合规检查**”与“**受控的批量修复**”做成一条可审阅、可回滚的闭环，减少重复性劳动项目背景： 针对工业化生产中海量资产导入引发的规范混乱等痛点，且传统批量工具极易引发内存溢出，主导开发底层管线安全管控插件。并降低改错成本。
● 项目背景： 针对工业化生产中海量资产导入引发的规范混乱等痛点，且传统批量工具极易引发内存溢出，主导开发底层管线安全管控插件。
● 核心成果：
摒弃传统硬加载方案，深度重构底层资产检索链路，在零内存溢出风险的前提下，将 200+ 复杂模型的全量规范扫描（面数/碰撞体/材质等风险项）耗时极速压缩至 1 秒以内。
● 关键问题解决： 
1. 设计「三段式 AI 安全工作流 (Plan JSON 契约 + Dry-Run)」，杜绝大模型幻觉对资产进行灾难性修改，实现修改操作的 All-or-Nothing。
2. 基于底层硬拓扑关系开发「依赖感知路由」，解决离散白模与材质在弃用模糊命名匹配后的精准成组与挂载难题。
本项目的核心定位不是“接一个 LLM”，而是把 AI 变成**可审计、可控爆炸半径、可拒绝、可回滚**的生产工具链能力。

---

## 核心设计与特点

### 1) 三段式闭环：Plan → Review → Execute

- 自然语言输入先转为 **Plan JSON**（步骤化、可验证、可留档）
- 执行前生成 **Dry-Run Diff**（before/after 变更预览，而不是黑盒改写）
- UI 支持“定位目标”以快速复核：
  - 关卡 Actor：Camera Focus
  - 纯资产：Sync to Content Browser

### 2) 安全门禁（避免批量写入失控）

- 工具能力分级：`ReadOnly / Write`
- 写操作必须走确认门禁（`requires_confirm`）
- 爆炸半径限制：`MAX_ASSET_MODIFY_LIMIT=50`
- SourceControl：启用时 Fail-Fast；未启用进入离线本地模式
- Transaction/Undo：批量修改可撤销（面向编辑器生产安全）

### 3) 大批量处理友好（尽量避免卡死编辑器）

- AssetRegistry 元数据扫描（避免无脑 Load）
- Slice-based 非阻塞扫描：进度查询 / 中断 / 重试
- 输出以“证据 + 摘要”为中心（适配审阅与报告）

---

## 覆盖的典型美术痛点（示例）

- 网格合规：StaticMesh 面数扫描（LOD0 triangle count）
- 关卡排雷：StaticMeshActor 缺失碰撞 / 默认材质风险检测 + 一键定位
- 命名归档：基于 `UAssetImportData/AssetUserData` 的自动前缀命名与批量 Move 归档
- 材质连线：基于命名契约的材质实例创建、贴图参数绑定与 Mesh 赋材（支持 Dry-Run 预览）

---

## 环境要求

- Windows
- Unreal Engine `5.4`
- 打开项目：`HCIEditorGen.uproject`

插件：
- `Plugins/HCIAbilityKit/`（双模块：Runtime + Editor）

---

## 快速演示（UE 控制台命令）

> 说明：以下命令用于“快速复现能力”。录视频时建议只挑 1～2 条最稳的链路展示即可。

### A. 不依赖 LLM 的稳定演示（推荐）

- `HCI.AuditScan`  
  使用 AssetRegistry 做同步元数据枚举（轻量）。
- `HCI.AuditScanAsync 200 10 1 10`  
  Slice-based 非阻塞扫描（`batch_size=200`，可选深度网格检查与周期 GC）。
- `HCI.AuditScanProgress` / `HCI.AuditScanAsyncStop` / `HCI.AuditScanAsyncRetry`
- `HCI.AuditExportJson <output_json_path>`  
  扫描并导出 JSON 报告（便于作品集展示“结果可留档”）。

### B. 打开 AI 辅助入口 UI（推荐录视频用）

- `HCI.AgentPlanPreviewUI "<自然语言>"`  
  触发规划并弹出 Plan Preview（含步骤证据与审阅/确认入口）。
- `HCI.AgentChatUI`  
  聊天式入口（历史 + 快捷指令 + 摘要链路）。

示例输入（可直接用于录制）：
- `扫描 /Game/Temp 下的资产，先给出合规风险摘要与可执行修复计划；所有写操作必须先给 Dry-Run Diff，我确认后再执行。`

### C. 工具能力声明（契约与白名单）

- `HCI.ToolRegistryDump`  
  打印冻结的 Tool Registry 能力声明（用于核对工具白名单与参数边界）。

---

## 可选：启用真实 LLM Provider（本地配置）

> 注意：请勿把 API Key 提交到仓库。

创建本地配置文件：
- `Saved/HCIAbilityKit/Config/llm_provider.local.json`

最小示例：

```json
{
  "api_key": "YOUR_KEY",
  "api_url": "https://YOUR_ENDPOINT",
  "model": "YOUR_MODEL_ID"
}
```

（可选）路由配置：
- `Saved/HCIAbilityKit/Config/llm_router.local.json`

---

## 仓库结构（阅读入口）

- 插件（核心）：`Plugins/HCIAbilityKit/`
  - Runtime：`Plugins/HCIAbilityKit/Source/HCIRuntime/`
  - Editor：`Plugins/HCIAbilityKit/Source/HCIEditor/`
  - Tests：`Plugins/HCIAbilityKit/Source/HCITests/`
- 文档（冻结口径与执行主线）：`Source/HCIEditorGen/文档/`
- Skills Bundle（`SKILL.md + prompt.md + tools_schema.json`）：`Source/HCIEditorGen/文档/提示词/Skills/`

推荐优先阅读：
- `Source/HCIEditorGen/文档/00_总进度.md`
- `Source/HCIEditorGen/文档/05_开发执行总方案_资产审计.md`
- `Source/HCIEditorGen/文档/提示词/AI可调用接口总表.md`

---

## 备注

- 该 Demo 强调：**契约冻结（Plan JSON + tools_schema）**、**工具白名单（Registry）**、**安全执行（门禁/事务/爆炸半径/源控）** 与 **批量处理稳定性**。
- 项目范围刻意收敛：不做“无确认自动改写资产”的黑盒工具；所有写入都必须可审阅与可回滚。
