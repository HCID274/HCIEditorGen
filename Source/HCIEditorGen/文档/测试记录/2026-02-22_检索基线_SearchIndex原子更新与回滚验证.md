# 测试记录：SearchIndex 原子更新与回滚验证

- 日期：2026-02-22
- 切片编号：历史基线增强（检索线一致性）
- 测试人：助手

## 1. 测试目标

- 将 `RefreshAsset/RemoveAssetByPath` 的统计与索引更新改造为原子流程，避免中途失败导致计数漂移或旧文档丢失。

## 2. 影响范围

- `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Search/HCIAbilityKitSearchIndexService.cpp`
- `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Tests/HCIAbilityKitSearchIndexAtomicTests.cpp`

## 3. 前置条件

- 增量统计实现已在 `UpdateDocumentStats` 生效。
- 可用本地构建命令与 `UnrealEditor-Cmd` 自动化测试入口。

## 4. 操作步骤

1. 修改 `RefreshAsset` 为“预检查冲突 -> 移除旧文档 -> 提交新文档 -> 失败回滚旧文档”。
2. 修改 `RemoveAssetByPath` 为“先删索引成功再扣减统计”，避免失败时计数先变更。
3. 新增自动化测试 `HCIAbilityKit.Editor.SearchIndex.AtomicRefreshRollback`，构造 duplicate id 冲突并验证旧数据仍保留、统计不变。
4. 新增自动化测试 `HCIAbilityKit.Editor.SearchIndex.AtomicRemoveStaleMappingGuard`，构造 stale mapping（路径映射保留但索引文档被移除）并验证 `RemoveAssetByPath` fail-fast 且统计不变化。
5. 执行编译：
   `Build.bat HCIEditorGenEditor Win64 Development -Project=... -WaitMutex -FromMSBuild`
6. 执行自动化：
   `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests HCIAbilityKit.Editor.SearchIndex.AtomicRefreshRollback+HCIAbilityKit.Editor.SearchIndex.AtomicRemoveStaleMappingGuard; Quit"`

## 5. 预期结果

- 冲突失败时 `RefreshAsset` 返回 false 且旧文档不丢失。
- 失败时 `IndexedDocumentCount/DisplayNameCoveredCount/SceneCoveredCount/TokenCoveredCount` 不变化。
- stale mapping 场景触发 fail-fast，不发生计数漂移。
- 两条新增自动化测试均通过。

## 6. 实际结果

- 通过：编译成功（ExitCode=0）。
- 通过：自动化测试日志显示 `AtomicRefreshRollback` 与 `AtomicRemoveStaleMappingGuard` 均执行完成，结果为“成功”。

## 7. 结论

- `Pass`

## 8. 证据

- 日志路径：`Saved/Logs/HCIEditorGen.log`（关键行包含 `Test Completed. Result={成功} Name={AtomicRefreshRollback}`）
- 代码路径：
  - `Plugins/HCIAbilityKit/Source/HCIAbilityKitRuntime/Private/Search/HCIAbilityKitSearchIndexService.cpp`
  - `Plugins/HCIAbilityKit/Source/HCIAbilityKitEditor/Private/Tests/HCIAbilityKitSearchIndexAtomicTests.cpp`
- 截图路径：无
- 录屏路径：无

## 9. 问题与后续动作

- 已补齐 stale mapping 负向用例，当前无新增阻塞项。
