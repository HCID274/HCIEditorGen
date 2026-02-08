# UE实践手册：编辑器工具链 MVP（HCIEditorGen / 战斗内容 AbilityKit 管线 / JSON Source-of-Truth + Reimport）

目标：把“做 Demo”升级为“做管线”。你要交付的是一条可复现的数据流：  
**JSON（权威来源）→ UE 导入/重导入（右键 Reimport）→ 派生资产（DataAsset/DataTable）→ Gameplay 读取生效**。

硬要求（P0）：
- **JSON 是唯一权威载体**：UE 内的派生资产只是“产物”，允许被 Reimport 覆盖，禁止手改当源头。
- **必须支持右键 Reimport**：打开 UE 后，内容浏览器里右键派生资产 → `Reimport` 能从 JSON 刷新并生效。
- **HTTP 可选但必须回退**：在线生成失败/超时/断网/脏数据 → 自动 fallback 到 Mock（编辑器不崩、不假写、不阻塞）。

前置条件（只做一次）：
- 先按 `10_UE实践/手册_环境准备.md` 完成 UE 5.4.4 / MSVC / Rider 安装与验证。

---

## 0. MVP 定义（必须严格遵守）

### 0.1 你要交付的是什么？
你交付的是“三件套”，缺一不可：
1) **SourceData（权威）**：一个可版本控制的 JSON 文件（示例：`SourceData/AbilityKits/HCI_Fire_01.hciabilitykit`）  
2) **派生资产（UE 内）**：一个可被 Reimport 的资产（`UHCIAbilityKitAsset`，DataAsset 形态）  
3) **编辑器入口（EUW）**：一键生成/更新 SourceData JSON（可选触发 Import/Reimport）

字段范围（先固定到“能验证战斗内容管线”，别膨胀）：
- `kit_id`（string）
- `display_name`（string）
- `abilities`（array）
  - `ability`（string，SoftClassPath：`UGameplayAbility`）
  - `level`（int）
  - `input_tag`（string，GameplayTag）
- `kit_tags`（array<string>，GameplayTag）
- `params.damage`（number，用于 Runtime 验收时做“可见变化”）

验收口径（你要能录屏证明）：
- 关闭 UE 也能改 JSON（为后续 QT 外部工具留路）
- 打开 UE：右键派生资产 `Reimport` → 资产内容更新 → PIE/运行时读取后生效
  - Runtime 验收入口与录屏脚本见：`10_UE实践/手册_LyraDogfooding底座.md`

### 0.2 明确不做（防止工具蔓延）
- 不做平台化（账号/云配置/批处理/搜索/权限）
- 不做 Runtime 本地推理硬集成（llama.cpp/LibTorch 放到 P2）
- 不做复杂 UI（EUW 只要能点、能看日志、能复现）

---

## 1. 数据流（本手册的最高规则）

唯一规则（写死）：
- **Source of Truth = JSON 文件（SourceData）**
- UE 内资产（DataAsset/DataTable） = **Derived**（派生，可被 Reimport 覆盖）

推荐目录（可按你的工程调整，但要固定下来）：
- `SourceData/AbilityKits/`：权威 JSON（可被 QT 外部工具编辑）
- `Content/HCIDuel/Tools/`：EUW、图标、调试素材
- `Content/HCIDuel/Data/AbilityKits/`：派生资产（`UHCIAbilityKitAsset`）

### 1.1 UE 插件开关（只做一次）
在 UE Editor 里打开插件（版本名称可能略不同，按关键词搜索即可）：
1. **Editor Scripting Utilities**（建议开启；EUW/编辑器脚本常用）
2. **Python Editor Script Plugin**（可选；如果你用 UE 内置 Python 做生成/校验）

启用后重启编辑器，验证：
- 你能创建 `Editor Utility Widget`
- Output Log 能跑最小脚本/打印日志（若启用 Python）

---

## 2. JSON Schema（输出格式锁死）

你生成/维护的 SourceData 文件必须符合以下 schema（字段名与类型固定）：

```json
{
  "kit_id": "HCI_Fire_01",
  "display_name": "Fire Kit 01",
  "kit_tags": ["Kit.Fire"],
  "params": {
    "damage": 35.0
  },
  "abilities": [
    {
      "ability": "/Game/HCIDuel/Gameplay/Abilities/GA_HCI_Attack.GA_HCI_Attack_C",
      "level": 1,
      "input_tag": "InputTag.HCI.Attack"
    }
  ]
}
```

校验与保护（P0 必须做）：
- 缺字段/类型不对：失败（不写派生资产）
- 数值 clamp：例如 `params.damage` 0-9999（先写死范围，后面再调）
- 写文件采用“先写临时文件再替换”（避免写一半断电/异常导致 JSON 损坏）

---

## 3. 派生资产的数据模型（Runtime）

> 目标：Runtime 只读派生资产，Gameplay 不直接解析 JSON（更稳、更接近真实项目）。

### 3.1 DataAsset（推荐，最贴合“资产管线”叙事）
创建一个 C++ DataAsset 类（示例命名）：
- `UHCIAbilityKitAsset : public UPrimaryDataAsset`

字段（先固定到 P0，可验证）：
- `FName KitId`
- `FText DisplayName`
- `FGameplayTagContainer KitTags`
- `float Damage`
- `TArray<FHCIAbilityGrant> Abilities`（`AbilityClass` + `Level` + `InputTag`）

要求：
- 全部 `UPROPERTY(EditAnywhere, BlueprintReadOnly)`，Reimport 后你能立刻在编辑器里看到变化。
- 加一个 `UAssetImportData* AssetImportData`（用于存源文件路径；右键 Reimport 依赖它）。

### 3.2 DataTable（备选：更省事，但叙事略弱）
如果你短期卡在自定义 Reimport 上，可以用 DataTable 临时兜底（UE 内置导入/重导入支持）。  
注意：**就算用 DataTable，也必须坚持 JSON 为权威来源**（别把数据改回表里当源头）。

---

## 4. Import/Reimport（P0 重点：右键 Reimport 必须可用）

目标：让 UE 认识你的 SourceData JSON，并把它变成一个可 Reimport 的派生资产。

推荐实现路径（更像工业，面试价值也最高）：
1) Runtime 模块：定义 `UHCIAbilityKitAsset`
2) Editor 模块（Editor-only）：实现 **Importer + Reimport**（Factory/ReimportHandler）

### 4.1 文件扩展名建议（避免“所有 .json 都被你接管”）
建议给 SourceData 用自定义扩展名，例如：
- `HCI_Fire_01.hciabilitykit`（内容仍然是 JSON 文本）

原因：
- UE 的 Factory 通常按扩展名匹配；直接接管 `.json` 会把范围搞太大。

### 4.2 P0 的最小验收步骤（你必须跑通）
1. 在 Windows 文件系统中准备一个 `*.hciabilitykit` 文件（内容符合 schema）。
2. 在 UE 内容浏览器里执行 Import（导入到 `Content/HCIDuel/Data/AbilityKits/`），生成一个 `UHCIAbilityKitAsset`。
3. 修改源文件内容（例如把 `params.damage` 改大，或把 `display_name` 改掉）。
4. 回到 UE：右键该资产 → `Reimport`。
5. 打开资产：字段更新，且 PIE/运行时读取后能生效（至少能在日志/验收入口看到变化）。

### 4.3 Reimport 的工程要求（P0 只做最小闭环）
- 失败必须给出清晰错误（Output Log + EUW UI）
- Reimport 过程不允许阻塞编辑器太久（超时/异常要可控）
- Reimport 不应“半写成功”：要么完整成功并保存，要么不改动派生资产

### 4.4 C++ 实现清单（把“你要写哪些东西”写死）
> 你不需要一开始写得很优雅，但必须先把“导入/重导入链路”跑通。

建议拆分（最常见的 UE 工业结构）：
- Runtime 模块（游戏运行时可用）：
  - `UHCIAbilityKitAsset`（数据模型 + `AssetImportData`）
- Editor 模块（只在编辑器编译，放到 `YourProjectEditor` 或一个 Editor-only 插件）：
  - `UFactory`：从 `*.hciabilitykit` 创建 `UHCIAbilityKitAsset`
  - Reimport 支持：实现 `FReimportHandler`（或 `UReimportFactory` 变体）
  - 关键点：把源文件路径写进 `AssetImportData`，这样内容浏览器右键才会出现/可用 `Reimport`

你跑通后至少要能做到：
- Import 一次能生成资产
- 改源文件后，右键 `Reimport` 能覆盖更新资产字段

---

## 5. EUW（编辑器面板）作为“入口”，不是作为“源头”

EUW 只做三件事：
1) 读取当前选中对象（AbilityKit 资产、或用于生成模板的角色/蓝图）
2) 生成/更新 SourceData JSON（权威）
3) 触发 Import/Reimport（可选；P0 至少要能提示你去右键 Reimport）

建议 EUW 最小 UI：
- 当前选中对象名称/路径（只读）
- 按钮：`Generate/Update AbilityKit (JSON)`
- （可选）按钮：`Import/Reimport Derived Asset`
- 多行文本：显示“最后一次生成的 JSON + 错误信息”（强烈推荐，利于录屏）

验收点（最小）：
- 你能录屏证明：选中一个 AbilityKit 相关对象（资产或模板蓝图），点按钮后 SourceData 文件被写出/更新（文件系统可见）

---

## 6. 生成逻辑（先假 AI 闭环，再上 HTTP；永远要回退）

### 6.1 P0：先做“假 AI”（强烈推荐）
第一天不要接任何外部 API：
- 用 deterministic 生成：基于对象名 hash 生成固定结果（可复现）
- 或本地模板库随机挑（也要能复现：记录 seed）

目的：你 1-2 天内必须完成闭环：  
**EUW → JSON → Import/Reimport → UE 资产更新**。

### 6.2 P0 可选：HTTP 在线生成（但必须断网回退）
当闭环稳定后，再把“生成 JSON”这一步替换为：
- UE C++ HTTP（或 UE Python requests）调用一个接口，要求严格返回 JSON

必须有的安全阀（写死）：
- 超时：例如 2-5 秒（失败立刻 fallback）
- 异常：捕获并 fallback（不允许卡死编辑器）
- JSON 校验失败：fallback（不写派生资产）

建议的“最低成本在线源”：
- 先用你本地 Python 起一个 HTTP 服务（Hello World 也行），验证链路与回退逻辑
- 等稳定后再换真实 LLM API

---

## 7. 里程碑（对齐 2026-02-23 Deadline）

### Day 1-2：数据模型 + Import/Reimport 跑通（右键 Reimport 必须成功）
- `UHCIAbilityKitAsset` 建好（字段固定 + ImportData）
- Factory/Importer 能从 `*.hciabilitykit` 导入并生成资产
- 右键 `Reimport` 跑通（核心验收）

### Day 3-4：EUW 入口 + JSON 写出 +（可选）触发 Reimport
- EUW 能读取选中对象
- 点按钮能写出/更新 SourceData 文件（含校验）
- 能指导/触发 Reimport，使派生资产刷新

### Day 5-7：HTTP 可选 + 回退做扎实（不影响稳定）
- HTTP 成功时写入 JSON
- HTTP 失败时自动 fallback（同一次点击就能落到离线结果）
- 编辑器稳定性与日志输出完善

---

## 8. 交付物（必须能用于投递/面试）

1) 30-60 秒录屏（必须包含右键 Reimport）：  
关闭 UE 改 JSON → 打开 UE 右键 Reimport → 资产更新 → PIE 读取生效
2) 1 页说明：数据流、Schema、回退机制（写在 README 或单独文档）
3) 1 条 STAR：你如何处理“超时/脏数据/稳定性”的工程取舍（写进 `10_UE实践/记录.md`）
