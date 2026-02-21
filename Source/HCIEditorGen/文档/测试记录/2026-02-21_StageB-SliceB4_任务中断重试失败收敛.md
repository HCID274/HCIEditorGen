# StageB-SliceB4 任务中断/重试/失败收敛（2026-02-21）

- 日期：2026-02-21
- 切片编号：StageB-SliceB4
- 测试人：Codex（实现/编译）+ 用户（UE 手测）
- 当前结论：`Pass`

## 1. 测试目标

- 在 B3“异步分片扫描”基础上补齐 B4 门禁能力：
  - 运行中可中断（Stop）；
  - 中断后可重试（Retry）；
  - 失败路径可收敛并给出可执行建议（包含参数非法路径）。

## 2. 影响范围

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/HCIAbilityKitEditorModule.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Audit/HCIAbilityKitAuditScanAsyncController.h`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Audit/HCIAbilityKitAuditScanAsyncController.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Tests/HCIAbilityKitAuditScanAsyncControllerTests.cpp`

## 3. 前置条件

- 项目：`D:\1_Projects\04_GameDev\UE_Projects\HCIEditorGen`
- UE：`5.4.4`
- B3 已 `Pass`
- 为触发 Stop 窗口，用户在 UE 内手动扩容测试资产至约 200+（本次为 209）

## 4. 操作步骤（已执行）

1. 代码实现：
   - 引入 `FHCIAbilityKitAuditScanAsyncController` 状态机；
   - 新增命令：
     - `HCIAbilityKit.AuditScanAsyncStop`
     - `HCIAbilityKit.AuditScanAsyncRetry`
   - 强化 `HCIAbilityKit.AuditScanProgress` 的状态输出（running/cancelled/failed/idle）。
2. 编译验证：
   - `Build.bat HCIEditorGenEditor Win64 Development -Project="D:/1_Projects/04_GameDev/UE_Projects/HCIEditorGen/HCIEditorGen.uproject" -WaitMutex -FromMSBuild`
3. UE 手测（用户）：
   - `t.MaxFPS = "5"`
   - `HCIAbilityKit.AuditScanAsync 1 20`
   - `HCIAbilityKit.AuditScanAsyncStop`
   - `HCIAbilityKit.AuditScanAsyncRetry`
   - 异常参数回归：`HCIAbilityKit.AuditScanAsync 0 20`

## 5. 预期结果

- 运行中 Stop 可生效，输出 `interrupted ... can_retry=true`；
- Retry 可复用上次上下文并重新开始扫描；
- 非法参数被拒绝并输出结构化错误；
- 失败或中断路径不产生脏状态。

## 6. 实际结果

- 编译通过（exit code 0）。
- 用户 UE 手测日志命中关键门禁：
  - `start total=209 batch_size=1`
  - `interrupted processed=9/209 can_retry=true`
  - `retry start total=209 batch_size=1`
  - 重试后持续进度：`progress=10% processed=21/209`、`progress=20% processed=42/209`
  - 参数非法收敛：`invalid_args reason=batch_size must be an integer >= 1`

## 7. 结论

- `Pass`
- 判定依据：
  - B4 核心链路 `Start -> Stop -> Retry` 已在 UE 实机触发并返回正确状态；
  - 失败收敛（参数非法）日志已命中；
  - 无异常崩溃或状态污染迹象。

## 8. 证据

- 用户 UE 日志关键片段：
  - `Cmd: HCIAbilityKit.AuditScanAsync 1 20`
  - `LogHCIAbilityKitAuditScan: Display: [HCIAbilityKit][AuditScanAsync] start total=209 batch_size=1`
  - `Cmd: HCIAbilityKit.AuditScanAsyncStop`
  - `LogHCIAbilityKitAuditScan: Display: [HCIAbilityKit][AuditScanAsync] interrupted processed=9/209 can_retry=true`
  - `Cmd: HCIAbilityKit.AuditScanAsyncRetry`
  - `LogHCIAbilityKitAuditScan: Display: [HCIAbilityKit][AuditScanAsync] retry start total=209 batch_size=1`
  - `LogHCIAbilityKitAuditScan: Display: [HCIAbilityKit][AuditScanAsync] progress=10% processed=21/209`
  - `LogHCIAbilityKitAuditScan: Display: [HCIAbilityKit][AuditScanAsync] progress=20% processed=42/209`
  - `Cmd: HCIAbilityKit.AuditScanAsync 0 20`
  - `LogHCIAbilityKitAuditScan: Error: [HCIAbilityKit][AuditScanAsync] invalid_args reason=batch_size must be an integer >= 1 usage=HCIAbilityKit.AuditScanAsync [batch_size>=1] [log_top_n>=0]`
- 构建日志：`C:\Users\50428\AppData\Local\UnrealBuildTool\Log.txt`

## 9. 问题与后续动作

- 后续切片切换到 `StageB-SliceB5`：采集 `triangle_count` 相关 Tags 并写入 `triangle_source=tag_cached` 口径。
