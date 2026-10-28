# AGENTS.md

本文件说明这个仓库的约定，供人类协作者与自动化 agent 共同遵守。

## 项目是什么

`mxd186 登录器`：Windows 上的冒险岛单机启动器。C++17 + 纯 Win32/GDI+，x86 静态链接（`/MT`），
无第三方库，产出单个 `dist\mxd186Launcher.exe`（带 `requireAdministrator` 清单）。

职责：起客户端 → 跳过启动页 → 检测「选完大区」→ 弹账号窗 → MySQL 校验 → 8800 认证 → 进游戏。

## 目录

```
src/        源码（main / ui / dlg / config / auth / game / match / util）
res/        app.rc —— 把 assets\backhome.png 编成 RCDATA 101
assets/     模板图等源资源
dist/       构建产物（唯一可执行文件）
tools/      调试与自检脚本（不属于运行时代码）
```

## 构建

```bat
build.bat
```

依赖 Visual Studio 2022（C++ 桌面工作负载）。脚本会：`vcvars32` → `rc` 编译资源 → `cl` 编译
→ 链接并嵌入清单 → 输出 `dist\mxd186Launcher.exe`。

- 只想调试界面又不想每次弹 UAC：`tools\build_asinvoker.bat`（输出 asInvoker 临时副本，别提交）。
- 改完必须实际编译通过再提交；涉及行为的改动要附上运行/日志证据。

## 代码约定

- 语言：C++17，`/utf-8`，源文件 UTF-8；面向 Windows API 用宽字符版本（`...W`）。
- 不引入第三方依赖；需要新库时先讨论，优先用系统 API。
- 资源用 RAII 之外一律成对释放（`CreateFile`/`CloseHandle`、`GetDC`/`ReleaseDC`）。
- 线程：网络/等待类工作放工作线程，UI 线程只做绘制；跨线程用 `PostMessageW`。
- 日志：`LogLine(L"...")` 写 `launcher.log`（GBK），关键分支必须留痕，便于现场排查。
- 注释写"为什么"，不写"是什么"；中文注释可以，代码标识符保持英文。
- 不提交任何私密信息：数据库口令、个人路径、抓包数据、构建中转文件。

## 提交规范：Conventional Commits

提交信息一律 **Conventional Commits**（cc）风格：

```
<type>(<scope>): <subject>

<body 可选，说明动机与影响>

<footer 可选，如 BREAKING CHANGE / Closes #12>
```

- `type`：`feat` `fix` `docs` `style` `refactor` `perf` `test` `build` `ci` `chore` `revert`
- `scope`（可选）：`ui` `dlg` `config` `auth` `game` `match` `build` `tools`
- `subject`：中文或英文皆可，祈使语气，不超过 50 字，句尾不加句号
- `body`：说明"为什么改、影响什么"，必要时列点
- 一个提交只做一件事；格式化与功能改动分开

示例：

```
feat(dlg): 登录成功后自动关闭第二窗体

- 成功提示停留 800ms 再关闭，失败保留窗口显示原因
- 便于玩家确认结果，也避免窗口压住游戏画面
```

```
fix(match): 用 NCC 替换 SAD 阈值判定

游戏登录界面的大面积同色背景会让 SAD 误命中；
改为归一化互相关 + 标准差门槛后误判消失。
```

## 许可

本项目采用 **署名-非商业性使用 1.0**（见 `LICENSE`）。提交代码即视为同意以同一许可发布。

- 任何分发（含二进制）必须保留 `LICENSE` 与署名信息，不得移除；
- 禁止商业用途；需要商用须先取得作者书面许可；
- 修改他人贡献时保留原作者署名，并在提交信息里说明来源。

## 分支与发布

- 主分支：`main`，保持可编译。
- 功能分支：`feat/<短描述>`、修复：`fix/<短描述>`。
- 版本号写进 README 的版本记录表；产物只在 `dist/` 更新，不额外放压缩包。

## 自检清单（提交前）

1. `build.bat` 干净编译，无新增警告。
2. 主窗：IP 校验、浏览客户端、打开配置文件可用。
3. 第二窗体：置顶跟随、账号密码校验、成功后自动关闭。
4. 模板匹配：`tools\build_match_test.bat` 后 `match_test -res` 返回 1。
5. 日志无异常堆栈；不把 `launcher.log`、`*.obj`、`*.res`、临时 exe 提交进去。
