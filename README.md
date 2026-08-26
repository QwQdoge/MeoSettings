# Meo Settings

Meo Settings 是 MeoArch 的日常系统设置应用。它以 MeoUI 提供的通用界面为基础，通过稳定的 Qt/KDE API、已安装的 MeoKDE 运行时和明确的 KCM 交接提供真实设置流程；它不取代 NetworkManager、BlueZ、PipeWire、KScreen、PowerDevil、KWin 或 System Settings。

## 目录 / Layout

- `qml/`：应用页面、导航和 MeoUI 组合。
- `src/`：Qt/C++ 后端、平台 API 与模型。
- `data/`：受版本控制的桌面文件、清单和应用数据。
- `tests/`：C++/QML 测试源码；`docs/`：随代码维护的公开架构、隐私和存储契约。
- `CMakeLists.txt`：应用、测试和安装规则。
- `out/`：既有本地构建内容，不能作为新内容入口；已归档的主题审计位于该项目的 Obsidian `04-validation/`。
- `.git/`、`.gitignore`：版本控制元数据和忽略规则。

根目录只新增入口文档、源码目录及必要构建/发布配置。不要新增 `plan.md`、`项目架构（第一版）`、审计副本、Agent 日志、临时截图或测试日志。已有的 `out/` 和历史根目录记录在审阅迁移前不移动、不删除，也不再接收新的生成物。

## 文件与记录边界 / File policy

`docs/` 只存放与当前实现直接绑定、需要公开维护的契约。所有计划、审计、决策记录、Agent 工作日志和历史报告统一存入：

`/home/shekong/Documents/Obsidian Vault/MeoArch/Projects/meo-settings/`

该目录必须有面向人的 `README.md`，说明记录目的、当前索引和关联交付物。不要在应用仓库保存其副本。

记录目录使用统一的编号结构：`00-inbox/`（临时收集）、`01-overview/`（范围与事实）、`02-decisions/`（已确认决定）、`03-work/`（计划和交接）、`04-validation/`（验证结论）和 `99-archive/`（已替代记录）。项目根 `README.md` 是人类入口；记录文件名使用 `YYYY-MM-DD--short-topic.md`，不要使用含混的“第一版”式名称。

持久生成物统一存入：

`/home/shekong/Projects/outputs/meo-settings/{build,install,validation,packages,tmp}/`

`build/` 放配置和编译结果，`install/` 放暂存安装树，`packages/` 放待发布包及校验资料，`validation/` 放可复查验证证据，`tmp/` 仅作可丢弃工作区。每次验证使用 UTC 运行标识 `YYYY-MM-DDTHHMMSSZ-short-label`，如 `validation/2026-08-26T104500Z-settings/`，并在里面保存 `README.md`、日志、测试/截图证据和环境说明。`tmp/` 是可丢弃空间，不能当作验收凭据。

## Product boundary

只使用可验证的 KDE/Qt/Meo.System 公共接口；原生控制没有完整安全和恢复路径时，应清晰交接到维护中的 KCM，而不是伪造本地状态。敏感、特权、破坏性或恢复相关操作必须保留在权威系统工具中。静态/离屏测试只能证明其覆盖的范围，不能替代真实 Plasma 会话和硬件验收。

## Read first

开始工作前先阅读 [AGENTS.md](AGENTS.md)，再阅读 `docs/` 中与改动相关的公共契约。通用 UI 改动应回到 `meo-ui`，并满足其 Showcase 的完整覆盖、构建、运行和证据要求。
