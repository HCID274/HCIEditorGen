# UE实践手册：Runtime Dogfooding 底座（HCIDuel / Lyra / AbilityKit 验收入口）

目标：用 **Lyra Starter Game** 作为“车能跑”的 Runtime 底座，让 `HCIEditorGen` 的派生产物（`UHCIAbilityKitAsset`）在运行时 **可应用、可触发、可验收、可出日志/报告**。

> 说明：本手册是 `00整体战略规划.md` 的 **13:00-18:00 项目块**执行清单之一；不提供独立排班表。

前置条件（只做一次）：
- 先按 `10_UE实践/手册_环境准备.md` 完成 UE 5.4.4 / MSVC / Rider 安装与验证。

---

## 0. 范围冻结（防止你被 Lyra 细节吞掉）

你在 Runtime 侧只做“验收入口”，不做玩法重写：
- 不重写 Lyra 的 Experience / GameFeature 大框架（最多做最小接入点）。
- 不做完整伤害/战斗系统扩展；只做 1 条“能观察到变化”的验证链路即可。
- 你卖的不是 Lyra，你卖的是：**AbilityKit 管线 + 工具产物的运行时验收**。

P0 验收口径（必须能录屏）：  
`SourceData/*.hciabilitykit` → Import/Reimport → `UHCIAbilityKitAsset` 更新 → PIE 中 `ApplyKit` 后立刻生效 → `Activate` 后可观察到变化（日志/属性/表现任一）。

---

## 1. 项目落点（你要写哪些 Runtime 代码）

### 1.1 数据模型（与工具链对齐）
- `UHCIAbilityKitAsset : UPrimaryDataAsset`
  - `FName KitId`
  - `FText DisplayName`
  - `FGameplayTagContainer KitTags`
  - `float Damage`（对齐 `params.damage`，用于“可见变化”）
  - `TArray<FHCIAbilityGrant> Abilities`（`AbilityClass` + `Level` + `InputTag`）
  - `UAssetImportData* AssetImportData`（供 Reimport，Runtime 不依赖）

### 1.2 应用与回滚（Dogfooding 必需：切 Kit 不能越切越脏）
做一个“Kit Applier”（组件或 UObject 工具类都行），要求：
- `ApplyKit(ASC, Kit)`：授予能力、附加标签、（可选）应用启动效果
- `ClearApplied()`：能撤销上一次 `ApplyKit` 造成的变更（能力 handle / effect handle / loose tags）
- 输出“验收日志”：本次授予了哪些 Ability、Damage 参数是多少、失败点在哪（资产缺失/Tag 非法等）

> 工具岗加分点：你能讲清 **为什么必须可回滚/可幂等**（避免内容团队试错把项目污染到不可收拾）。

---

## 2. 最小验收入口（P0：先用 Console/Exec，不做 UI）

目标：面试时能做到“当场改 JSON→Reimport→PIE→一条命令验证”。

建议做 2 个 Exec 命令（任意载体：PlayerController / CheatManager / Subsystem）：
1. `HCI.ApplyKit <AssetPathOrId>`：加载并应用某个 `UHCIAbilityKitAsset`
2. `HCI.ActivateTag <GameplayTag>`：用 `ASC->TryActivateAbilitiesByTag(...)` 触发能力（不依赖输入系统）

P0 观测点（至少 1 个必须稳定）：
- 日志：Ability 被激活，打印 `Damage` 等关键参数
- 或属性：Health 等数值发生可控变化
- 或表现：播放一个 Montage/特效（只要能稳定证明“kit 生效”即可）

---

## 3. 最小验证场景（Map/Actor）

创建一个专用验证地图（示例命名）：`HCI_KitValidation`
- 目标：进入 PIE 后 10 秒内完成一次“Apply + Activate + 观察输出”
- 放一个“靶子/假人”（哪怕只是被攻击时打印日志），用于展示技能对目标产生影响

加分但不强制：
- 把“本次验收结果”写到一个可复制的文本区（屏幕日志/本地文件均可）

---

## 4. 录屏脚本（用于 90 秒作品视频的 Runtime 段落）

你需要固定一个“可复现流程”，每次改工具都能复录：
1. 关掉 UE，改 `SourceData/AbilityKits/HCI_Fire_01.hciabilitykit`（把 `params.damage` 改大）
2. 打开 UE，右键派生资产 `Reimport`（录到）
3. PIE：执行 `HCI.ApplyKit ...`（或自动应用默认 kit）
4. PIE：执行 `HCI.ActivateTag InputTag.HCI.Attack`
5. 观测变化（日志/属性/表现），并在画面里留下证据（Output Log/屏幕日志/属性面板）

---

## 5. 与工具链对齐的“不要做清单”（常见翻车点）

- 不要让 Runtime 直接解析 JSON（面试官会认为你绕开了 Import/Reimport 的工业链路）
- 不要让切 Kit 只增不减（一次演示看不出来，多次演示必翻车）
- 不要把输入系统当 P0（P0 用 Exec/Console 触发；输入映射放 P1）
