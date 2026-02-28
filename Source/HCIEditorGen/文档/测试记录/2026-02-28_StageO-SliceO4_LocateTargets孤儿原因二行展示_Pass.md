# 2026-02-28 Stage O-SliceO4 UE 手测记录（Pass）

## 目标

- 当 `AutoMaterialSetupByNameContract` 在严格契约下失败/退出时：
  - 仍能给出“可定位”的孤儿/未解决资产列表（LocateTargets）。
  - 每条定位项展示“为什么不通过”的原因（二行展示），便于美术立刻知道该改名还是补配对。
- 写操作必须保持 HITL：未点击“通过”前不得修改资产。

## 前置条件

- 引擎：UE 5.4.4
- 测试目录根：`/Game/__HCI_Test`

## 步骤

1. 执行：`HCIAbilityKit.MatLinkReset`
2. 打开：`HCIAbilityKit.AgentChatUI`
3. 输入（严格契约，S2 预期失败）：  
   `扫描 /Game/__HCI_Test/Incoming/MatLinkChaos/S2_Fuzzy，然后按契约自动创建材质实例并挂贴图`
4. 观察：
   - 审批卡（DryRun 预览）失败原因
   - LocateTargets 是否出现
   - LocateTargets 每项是否包含“原因第二行”
   - 点击任意定位项，是否能在 Content Browser 定位资产

## 预期

- 审批卡提示明确失败原因（例如未命中严格契约 / 缺失贴图配对），且未执行真实写操作。
- 聊天窗口出现 `结果定位（N 项，可点击跳转）`。
- 每条定位项：
  - 第 1 行为可点击按钮
  - 第 2 行为原因短句（例如：后缀不满足契约 / 缺少配对 Mesh / Mesh 缺少 BC/N/ORM）

## 结果（证据）

- LocateTargets 出现，且定位项来源为：
  - `AutoMaterialSetupByNameContract/orphan_assets`
  - `AutoMaterialSetupByNameContract/unresolved_assets`
- 点击定位项能够定位资产到 Content Browser（例如 `SM_Old_wooden`）。
- 审批卡文案包含“仅 DryRun 预览，不会修改资产；需要通过确认后才会修改资产”的门禁语义。

## 结论

- `Stage O-SliceO4` UE 手测：`Pass`。

