# 测试记录：Phase3-SliceP3-2_GlobalStep2_ToolActions_P3P4_CommonSupportAndCleanup（Pass）

日期：2026-02-28  
门禁：`Pass`（用户 UE 手测）  
范围：全局 Step 2（ToolActions）

## 1. 目标

- P3：在 `AgentActions/Support/` 下提取 ToolActions 公共深模块（参数读取与校验 / Evidence 拼接 / 批处理循环外壳），让 10 个 `ToolAction.cpp` 更薄、更易读。
- P4：清理战场：`HCIAbilityKitAgentToolActions.cpp` 仅保留 `BuildStageIDraftActions` 工厂注册；未接线/遗留旧实现物理隔离到 `Private/AgentActions/Experimental/`（或从主线目录剥离）。
- 约束：`No Semantic Changes`（业务语义绝对不变，仅结构迁移与调用路径收敛）。

## 2. 前置条件

- UE 工程可打开且插件已编译：`HCIEditorGenEditor Win64 Development`。

## 3. 验收步骤（UE 手测）

在 UE 控制台执行：

1. 打开 ChatUI：`HCIAbilityKit.AgentChatUI`
2. 只读链路（应直接完成并给出简洁结果摘要）：
   - `统计 /Game/Temp 的模型面数`（`ScanMeshTriangleCount`）
   - `检查当前关卡全场景的碰撞和默认材质`（`ScanLevelMeshRisks`）
   - `搜索 MNew 目录`（`SearchPath`，确保回复不重复）
3. 写链路（应按审批卡流程，dry-run 可通过）：
   - `整理并归档 /Game/Temp 临时资产 到 /Game/Art/Organized`（`NormalizeAssetNamingByMetadata` 先 dry-run）

## 4. 预期结果

- 10 个 ToolAction 全部可被规划/执行（不新增工具、不改变 schema）。
- UI 行为不退化：无重复气泡、摘要可读性不下降、定位按钮正常。
- 不出现新增错误码；原有错误码语义不变。

## 5. 实际结果

- 结论：`Pass`
- 证据：
  - 用户 UE 手测口头反馈：`Pass`
  - 手测覆盖：ChatUI + 只读/写链路（见“验收步骤”）

## 6. 备注

- 若出现 `Fail`，需记录具体 tool_name / error_code / evidence 关键字段。
