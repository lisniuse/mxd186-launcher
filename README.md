# mxd186 登录器

一个自用的冒险岛单机启动器：**纯 Win32 + GDI+**，C++17，x86 静态链接，无第三方库、无运行库依赖，
单个 exe 分发。负责「起客户端 → 跳过启动页 → 等玩家选完大区 → 弹账号窗 → 校验 → 认证 → 进游戏」。

---

## 1. 目录结构

```
Mxd186Launcher\
├─ build.bat                    一条命令构建：rc 资源 → 编译 → 输出 dist\mxd186Launcher.exe
├─ README.md                    本文件
├─ src\
│  ├─ main.cpp                  入口：单实例互斥 + GDI+ 初始化 + 读配置 + 起主窗
│  ├─ ui.h / ui.cpp             主窗：无边框分层窗口(UpdateLayeredWindow) + 自绘控件
│  ├─ dlg.h / dlg.cpp           第二窗体「mxd186 账号登录」
│  ├─ config.h / config.cpp     配置读写（用户目录 ini）+ 默认值生成 / 迁移
│  ├─ auth.h / auth.cpp         MySQL 账号校验 + 8800 认证
│  ├─ game.h / game.cpp         启动客户端、等窗口、跳过启动页、杀旧进程
│  ├─ match.h / match.cpp       模板匹配（判断「选完大区」）
│  ├─ util.h / util.cpp         GBK↔UTF16、ini(GBK)、注册表、日志、路径
│  └─ app.manifest              requireAdministrator
├─ res\app.rc                   RCDATA 101 = assets\backhome.png（模板编进 exe）
├─ assets\backhome.png          模板图（110×41）
├─ dist\mxd186Launcher.exe      **唯一产物**
└─ tools\                       调试/自检脚本（见第 8 节）
```

产物只有一个文件；运行配置放在用户目录，不跟 exe 走。

---

## 2. 运行流程

```
主窗（mxd186 登录器）
 ├─ 服务器 IP：手填（默认取配置里的 serverip）
 ├─ 游戏客户端：点「浏览」选 MapleStory.exe，或首次运行自动探测
 ├─ 打开配置文件：记事本打开 ~\.Mxd186Launcher\fig.ini
 └─ 开始游戏
      ├─ 校验 IPv4 合法
      ├─ 把 IP / 客户端写回配置
      ├─ CreateProcess("MapleStory.exe <IP> 8484", cwd = 客户端目录)
      ├─ 等 StartUpDlgClass → 发 WM_CLOSE 跳过启动页
      └─ 等 MapleStoryClass（游戏主窗口）
           ├─ 隐藏主窗
           ├─ 起检测线程：抓游戏客户区 → 找「回到首页」按钮
           └─ 连续两帧命中 → 弹「mxd186 账号登录」
                ├─ 窗口置顶，每 200ms 重新居中到游戏窗口
                ├─ 输账号密码 → 点「登录」
                │    ├─ MySQL 校验
                │    ├─ 写 HKCU\SOFTWARE\Maple\{name,login,pawd}
                │    ├─ 连 <IP>:8800 发「账号\r\n密码\r\naoa\r\n + 环境块」→ 收 "A\n"
                │    └─ 成功后提示 800ms → 窗体自动关闭
                └─ 客户端自己走完 8484 → 7575，进游戏
```

要点：

- 第二窗体**只在检测到「回到首页」按钮后**才弹，无配置开关；模板缺失/超时（15 分钟）则兜底弹窗。
- 账号密码不在主窗收集，全部在第二窗体输入。
- 8800 只在「游戏正等登录」的时间窗内应答；游戏没开时同样报文没有回包，属服务端行为。

---

## 3. 构建

需要 **Visual Studio 2022**（含 C++ 桌面工作负载）。

```bat
cd Mxd186Launcher
build.bat
```

`build.bat` 做的事：`vcvars32` → `rc /fo res\app.res res\app.rc` → `cl`（`/MT`、`/utf-8`、x86）
→ 链接 `app.res` 与 `requireAdministrator` 清单 → 输出 `dist\mxd186Launcher.exe`。

调试时不想每次弹 UAC，可用 `tools\build_asinvoker.bat` 编一个不需要管理员权限的临时副本。

---

## 4. 配置文件

位置：**`%USERPROFILE%\.Mxd186Launcher\fig.ini`**（GBK）

- 首次运行自动创建（带注释的默认配置）；
- exe 旁边若存在旧 `fig.ini`，第一次运行会复制过去；
- 主窗「打开配置文件」即用记事本打开它。

```ini
[fig]
fig1=D:\dgames\MXD186NEW\Client   ; 客户端目录（必需，里面要有 gameexe）

[bms]
gameexe=MapleStory.exe            ; 客户端 exe 名
serverip=127.0.0.1                ; 主窗默认 IP（界面上还能改）
serverport=8484                   ; 传给客户端的端口
auth=mysql                        ; mysql / tcp / none
authport8800=8800                 ; 点「登录」时连的端口
waitgame=60000                    ; 等游戏窗口超时(ms)
killgame=1                        ; 启动前杀掉已有的 MapleStory

[mysql]
host=auto                         ; auto 或留空 = 用主窗里填的 IP
port=3306
user=root
password=root
db=bms
```

可选/兼容键（写了就生效，程序不自动写回）：
`bindip`、`autotype`、`dlgdelay`、`dlgchild`、`website`、`[biaoti] biaoti1 / 自动登录`、`[mysql] sql`。

`fig1` 无效或为空时自动探测：`exe\Client` → `exe\..\Client` → `exe\..\..\Client` → exe 同目录。

---

## 5. 关键实现

### 5.1 主窗（新粗野主义 UI）

- 无边框分层窗口，GDI+ 全自绘 + `UpdateLayeredWindow`（失败自动退回普通窗口绘制）；
- 视觉：米黄底 + 粉色标题块 + 粗黑描边 + **硬阴影**（`BrutalBox()`，不模糊）+ 无圆角 + 全加粗；
- 控件：`服务器 IP`、`游戏客户端`（+ 浏览 / `GetOpenFileNameW`）、`打开配置文件`、`开始游戏`；
- 交互：Enter = 开始游戏；IP 框只收数字与点；标题栏可拖动；右上 ✕。

### 5.2 启动与跳过启动页

```cpp
CreateProcessW(L"\"MapleStory.exe\" <ip> <port>", cwd = clientDir);
WaitGameWindow(L"StartUpDlgClass") → PostMessageW(dlg, WM_CLOSE);   // 跳过启动页
WaitGameWindow(L"MapleStoryClass");                                 // 主窗口
```

### 5.3 第二窗体

- `WS_POPUP` 无系统标题栏，自绘 30px 标题条（可拖动，右上 ✕）；
- 顶层置顶 + 每 200ms `FollowGameWindow()` 重新居中、重新置顶；
- 可选 `WS_CHILD` 挂成游戏子窗口（`dlgchild=1`）——D3D 下会闪烁，默认关闭；
- 登录成功 800ms 后自动关闭；失败原地显示原因。

### 5.4 模板匹配（判断「选完大区」）

```
抓图：屏幕 DC BitBlt 客户区  →  全黑退 PrintWindow  →  仍黑退窗口 DC
预处理：BGRA → 灰度 → 4 倍下采样
搜索：积分图求窗口均值/方差 + NCC（归一化互相关）滑窗
判定：NCC ≥ 0.80 且连续两帧命中
```

- 选 NCC 而不是 SAD：游戏界面大面积同色背景会让 SAD 误判；NCC 对亮度不敏感，只比纹理。
  自检实测：有按钮 1.00 / 0.92，无按钮 0.17。
- 模板以 `RCDATA 101` 编进 exe；若 exe 旁有 `assets\backhome.png` 则优先用外部文件（换图免重编译）。

### 5.5 账号校验与 8800 认证

- 校验：动态加载 `libmysql.dll`（依次尝试 exe 旁、`..\Server\phpStudy\MySQL\lib\` 等路径），
  对 `accounts` 表执行 `SELECT id ... name/password`；
- 8800：`connect <ip>:8800` → `send "账号\r\n密码\r\naoa\r\n" + 环境块` → `recv "A\n"`；
- 环境块由 `BuildEnvBlock()` 现场枚举本机进程生成（`进程名\0路径\0…`），服务端不校验其内容。

---

## 6. 已知限制

- **独占全屏**下屏幕拷贝可能拿到黑图；会退 `PrintWindow`，两者都黑则检测失败 → 15 分钟兜底弹窗。
  窗口化 / 无边框窗口游戏不受影响。
- 子窗口模式（`dlgchild=1`）在 D3D 下会闪烁，默认关闭。
- 8800 有频率限制：短时间内反复点「登录」可能没有回包，稍等再试。
- 跨机使用需在服务器那台放行入站 `8484 / 8800 / 7575`。
- 客户端本体带保护壳，本项目只做进程级配合（起进程 / 发窗口消息 / 写注册表），不注入、不调试。

---

## 7. 日志

`launcher.log` 写在 `%USERPROFILE%\.Mxd186Launcher\launcher.log`（GBK，与 `fig.ini` 同目录），关键行：

```
已启动: "MapleStory.exe" 127.0.0.1 8484 (pid=...)
跳过启动页: WM_CLOSE -> StartUpDlgClass
[个性登录] 窗口已显示 hwnd=... 位置=(...) 大小=...
[个性登录] 正在登录…
8800: 现场生成环境块 N 字节
8800 回包 2 字节: 41 0a
[个性登录] 8800 认证: A(通过)  回包=A
[个性登录] 登录成功，窗体自动关闭
```

---

## 8. tools 里的脚本

| 文件 | 用途 |
|---|---|
| `build_asinvoker.bat` | 编一个不需要管理员权限的临时副本（截图 / 自测） |
| `build_match_test.bat` + `match_test.cpp` | 模板匹配自检：`match_test -res` 验内嵌模板；`match_test <png> <hwnd>` 对指定窗口匹配 |
| `dump_8800.js` | Frida：抓 8800 报文的 connect / send / recv（附落盘） |
| `run_frida_elevated.ps1` | 提权 attach 并写日志（登录器是 requireAdministrator） |
| `capture_8800_elevated.ps1` | pktmon 抓 8800 端口（备用；注意 16MB 文件上限） |
| `verify_handoff.js` | 观察进程是否对子进程动手（注入 / 钩子 / 窗口消息 / 共享内存） |
| `watch_click.ps1` / `watch_click2.ps1` | 注册表 / 文件 / 连接 / 进程四通道监视（带动作标记） |
| `snapshot_launch.ps1` | 启动前后环境快照 |
| `shot_*.png` | 最新界面截图（主窗 / 第二窗体） |

---

## 9. 版本记录

| 版本 | 内容 |
|---|---|
| 0.1 | 主窗 + 起游戏 + 跳过启动页 + 第二窗体骨架 |
| 0.2 | 账号校验（libmysql）、写注册表、8800 认证、第二窗体自动关闭 |
| 0.3 | 主窗职责收敛为「只填 IP」；新增客户端浏览；配置搬到用户目录并首次自动生成 |
| 0.4 | 第二窗体改为「匹配『回到首页』才弹」；匹配算法换成 NCC；窗口置顶跟随居中 |
| 0.5 | 整体改新粗野主义 UI；产物收敛为 `dist\mxd186Launcher.exe` 单文件 |

---

## 10. 用途

本机 / 局域网自用与学习（Win32 自绘界面、进程协作、模板匹配、协议实现）。请勿对外传播产物或抓包数据。
