# 测试记录：Phase6-SliceP6-1_GlobalRenaming（待验证）

- 日期：2026-03-01
- 切片：`Phase6-SliceP6-1_GlobalRenaming`
- 目标：
  - 全工程命名瘦身：`HCIAbilityKitAgent/Audit/Search/HCIAbilityKit` 前缀按映射规则统一缩写。
  - 同步物理文件与模块命名：`HCIAbilityKitRuntime/Editor/Tests` -> `HCIRuntime/HCIEditor/HCITests`（含 `.uplugin` 注册与依赖引用）。
  - 配置极简：不使用 `[CoreRedirects]`（按“零历史包袱”策略移除重定向；如未来出现资产断链，再按需引入）。
  - 兼容性：保持既有配置目录约定 `Saved/HCIAbilityKit/Config/*` 不变，避免 `llm_config_missing(E4307)`。
  - ChatUI：可执行计划不再阻塞等待 LLM 摘要回包。

## 1. 构建验证（本地）

- 命令：
  - `D:\Unreal Engine\UE_5.4\Engine\Build\BatchFiles\Build.bat HCIEditorGenEditor Win64 Development -Project="D:\1_Projects\04_GameDev\UE_Projects\HCIEditorGen\HCIEditorGen.uproject" -WaitMutex -NoHotReloadFromIDE`
- 预期：
  - 编译与链接成功（exit code 0）。
- 结果：
  - 通过（exit code 0）。

## 2. UE 门禁（手测）

- 前置建议：
  - 清理缓存并重新生成项目文件（模块名/文件名已批量更改）。
- 结论：`待验证`
