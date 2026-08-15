# CET 项目安全审计与代码审查报告

> 审查对象：`d:/Projects/CET`（C / raylib 5.0 / CMake 单机小游戏项目）
> 审查方式：只读静态代码审查。未对任何项目代码做添加、修改、删除或重构，报告中仅含问题描述与文字化修复建议，不含任何代码补丁。
> 审查范围：全部 `main.c`、`src/`、`include/` 源码与头文件，`CMakeLists.txt`、`CMakePresets.json`、`cmake/Findraylib.cmake`、`.gitignore`、`docs/` 架构文档，并对危险模式（`strcpy/strcat/system/popen/sprintf/scanf/malloc/realloc/free/fopen/memcpy/rand/srand` 等）做了全量搜索。

---

## 一、总体结论

本项目为**本地单机、无网络、无数据库、无可信边界输入**的 raylib 游戏。基于当前代码形态：

- **未发现**可被外部或远程利用的经典高危漏洞。SQL 注入、命令/脚本注入、XSS、CSRF、SSRF、路径遍历、不安全的反序列化、硬编码密钥/密码、弱加密协议等攻击面在本项目中**均不适用**（无相应输入通道），此项已在 3.0 节逐项说明，避免“未被审查”的误解。
- **主要风险集中在**：内存安全与资源管理（`realloc` 未检查、除零、延迟请求队列的再入与资源释放路径）、逻辑正确性（相机算法缺陷、地面魔法数重复、RNG 缺陷、死代码/接口不一致）、以及构建期供应链（未固定 SHA 的 `FetchContent` + 硬编码本机绝对路径）。
- 代码整体结构清晰、注释充分，场景栈 + 延迟请求的设计思路正确（有效规避了场景回调中修改栈导致的 use-after-free），值得肯定。

按严重程度分级：

| 级别 | 数量 | 说明 |
|------|------|------|
| 高 | 0 | 无经典高危漏洞（攻击面受限所致） |
| 中 | 8 | 内存安全、崩溃风险、逻辑缺陷、弱随机、供应链 |
| 低 | 12 | 死代码、健壮性、可维护性、代码卫生 |

---

## 二、最需要优先修复的安全与稳定性问题（按优先级排序）

> 以下为建议优先处理的项目，完整清单见第三节。

1. **【中｜内存安全】`realloc` 返回值未检查 → 空指针解引用与内存泄漏** — [`src/core/gamestack.c`](src/core/gamestack.c:42)（`EnsureSceneCapacity`）与 [`src/core/gamestack.c`](src/core/gamestack.c:51)（`EnsurePendingCapacity`）
   - 场景栈与延迟请求队列是每帧都使用的核心数据结构，扩容时若 `realloc` 失败返回 `NULL`，会**覆盖原指针**（原内存泄漏）且后续 `stack->scenes[stack->size++] = scene`（[`src/core/gamestack.c`](src/core/gamestack.c:78)）对 `NULL` 解引用直接崩溃。
   - 触发条件：内存不足；长时间运行反复增容时概率虽低，但属于典型的“一旦触发即崩溃 + 泄漏”组合缺陷。
   - 修复建议：检查 `realloc` 返回值，失败时保留旧指针、回滚容量并返回错误（或走安全降级），不得静默继续。

2. **【中｜链接缺陷】`genRandomNum` 声明与实现不一致** — [`include/tools/genrandom.h`](include/tools/genrandom.h:9) vs [`src/tools/genrandom.c`](src/tools/genrandom.c:3)
   - 头文件声明 `int genRandomNum(int range)`，但实现定义的是 `static int genRandomNumber(const int)`（静态、无外部符号）。一旦任何代码调用该 API 即产生**链接期 undefined reference**；当前因无人调用而侥幸通过构建（属死代码）。
   - 修复建议：统一函数名、去掉 `static` 使其真正可链接，或整体删除该模块。

3. **【中｜崩溃风险】动画初始化 `frameCount` 为 0 时整数除零** — [`include/tools/animation.h`](include/tools/animation.h:26)
   - `frameWidth = texture->width / frameCount` 未校验 `frameCount`，传 0 时整数除零触发未定义行为（x86 上 SIGFPE 崩溃）。当前调用方均传正值（8/4/1/4），属潜在缺陷。
   - 修复建议：初始化时校验 `frameCount > 0` 与纹理有效，非法输入回退默认值并报错。

4. **【中｜供应链】构建期依赖与路径可投毒** — [`CMakeLists.txt`](CMakeLists.txt:30)（`FetchContent` 以 tag `5.0` 拉取，未固定 commit SHA、无完整性哈希）、[`CMakeLists.txt`](CMakeLists.txt:15)、[`CMakePresets.json`](CMakePresets.json:12)、[`cmake/Findraylib.cmake`](cmake/Findraylib.cmake:12)（硬编码 `D:/RayLib_MinGW`、`D:/MinGW64` 等本机绝对路径）
   - 构建时联网下载并编译第三方源码；若 `D:/RayLib_MinGW` 等路径被替换为恶意库，链接时即可引入任意代码。同时绝对路径外泄本机目录布局、降低可移植性与可复现性。
   - 修复建议：`FetchContent` 固定到具体 commit SHA（必要时加哈希校验）；本机路径改用环境变量 / CMake 缓存变量注入，不写死提交进仓库。

5. **【中｜弱随机】`srand(time(NULL))` 每次调用重播种 + 模偏差** — [`src/tools/genrandom.c`](src/tools/genrandom.c:4)
   - 每次调用都重新播种，同一秒内多次调用返回相同序列（且污染全局 RNG 状态）；`rand() % range` 存在模偏差。若该函数将来用于词库抽选/奖励等，结果可预测、分布不均。
   - 修复建议：仅进程启动播种一次，或改用更高熵的现代 PRNG，并用拒绝采样消除模偏差。

6. **【中｜逻辑缺陷】相机“落地平滑”目标线算法不生效** — [`src/tools/camera.c`](src/tools/camera.c:53)
   - `targetY` 每帧由常量表达式重算（`(logicHeight - evenOutMaxY*logicHeight) - evenOutSpeed*dt`），随后又被钳制到 `evenOutMaxY * logicHeight`；结果 `evenOutSpeed` 与 `dt` 完全不产生累积下移效果，目标线退化为固定值，与注释描述的“目标线下移”行为不符。
   - 修复建议：用状态变量累积目标线位置（每帧 `targetLine -= evenOutSpeed*dt`），而非每帧从常量重算。

7. **【中｜逻辑缺陷】地面高度魔法数重复且跨文件硬编码** — [`src/player.c`](src/player.c:179)（`480 - 50`）与 [`src/scenes/scene_test.c`](src/scenes/scene_test.c:72)（`logicHeight - 50`）、[`src/game.c`](src/game.c:7)（逻辑分辨率）
   - 逻辑高度 480 与地面偏移在三处各自硬编码、相互独立。一旦调整分辨率或缩放，碰撞顶面与绘制将错位（玩家穿地/悬浮）。
   - 修复建议：由统一配置（如 `game_config.h` 或 `GameApp`）派生逻辑高度与地面 Y，消除重复魔法数。

8. **【中｜资源泄漏 / 再入】延迟请求队列的默认分支与 flush 期再入** — [`src/core/gamestack.c`](src/core/gamestack.c:147)（default 分支直接 `free(scene->data)` 而不调用 `onExit`，场景持有的 GPU 纹理/音频将泄漏）、[`src/core/gamestack.c`](src/core/gamestack.c:128)（flush 循环每次重读 `pendingSize`，flush 期间场景回调新入队的请求会在同一轮被消费，改变“下一帧应用”语义，极端顺序下可能出现“先 onResume 后立即销毁”的状态不一致）
   - 修复建议：default 分支也应走 `onExit` 后释放或断言拒绝未知类型；flush 前快照请求数，只处理快照范围内的请求，新请求留待下一帧。

---

## 三、问题清单（按严重程度）

### 3.1 中等级（建议优先修复）

| # | 位置 | 类型 | 问题描述 | 触发条件 | 潜在影响 |
|---|------|------|----------|----------|----------|
| M1 | [`src/core/gamestack.c`](src/core/gamestack.c:42)、[`src/core/gamestack.c`](src/core/gamestack.c:51) | 内存安全 | `realloc` 返回值未检查 | 内存不足 / 反复增容 | OOM 时泄漏原内存并对 NULL 解引用崩溃 |
| M2 | [`include/tools/genrandom.h`](include/tools/genrandom.h:9)、[`src/tools/genrandom.c`](src/tools/genrandom.c:3) | 链接缺陷 | 声明 `genRandomNum` 与实现 `static genRandomNumber` 不一致 | 一旦被调用 | 链接期 undefined reference；当前为不可用死代码 |
| M3 | [`include/tools/animation.h`](include/tools/animation.h:26) | 崩溃风险 | `frameCount == 0` 时整数除零 | 调用方误传 0 | UB / SIGFPE 崩溃 |
| M4 | [`CMakeLists.txt`](CMakeLists.txt:30)、[`CMakeLists.txt`](CMakeLists.txt:15)、[`CMakePresets.json`](CMakePresets.json:12)、[`cmake/Findraylib.cmake`](cmake/Findraylib.cmake:12) | 供应链 | `FetchContent` 未固定 SHA；本机绝对路径写死 | 构建期联网拉取 / 路径被替换 | 投毒引入任意代码、不可复现、外泄目录布局 |
| M5 | [`src/tools/genrandom.c`](src/tools/genrandom.c:4) | 弱随机 | `srand(time(NULL))` 每调重播种 + `%range` 模偏差 | 高频调用 / 用于抽选 | 随机结果可预测、同秒重复、分布不均 |
| M6 | [`src/tools/camera.c`](src/tools/camera.c:53) | 逻辑缺陷 | 落地平滑目标线被钳制为固定值，`evenOutSpeed` 不生效 | 使用 `CAMERA_FOLLOW_EVEN_OUT_LANDING` | 相机平滑行为与设计不符（画面跳跃感） |
| M7 | [`src/player.c`](src/player.c:179)、[`src/scenes/scene_test.c`](src/scenes/scene_test.c:72) | 逻辑缺陷 | 地面高度/逻辑分辨率魔法数跨文件重复硬编码 | 调整分辨率 | 碰撞与绘制错位 |
| M8 | [`src/core/gamestack.c`](src/core/gamestack.c:147)、[`src/core/gamestack.c`](src/core/gamestack.c:128) | 资源/再入 | default 分支绕过 `onExit` 释放 data；flush 期间消费新入队请求 | 未知请求类型 / 场景回调内再次切换 | GPU 资源泄漏；状态机时序不一致 |

### 3.2 低等级（建议择机处理）

| # | 位置 | 类型 | 问题描述 |
|---|------|------|----------|
| L1 | [`src/tools/strings.c`](src/tools/strings.c:22) | 资源语义 | `freeString` 为 no-op 且无拥有者模型，易被误用为“已释放内存”，造成静默泄漏；且整个 `strings` 模块为未使用死代码（作者自注“不知道写了有什么用”） |
| L2 | [`src/tools/strings.c`](src/tools/strings.c:3) | 类型安全 | `toString` 将 `const char *` 强转 `char *` 并返回可变指针，写穿字符串字面量属 UB |
| L3 | [`src/core/gameapp.c`](src/core/gameapp.c:11)、[`src/core/gameapp.c`](src/core/gameapp.c:27) | 健壮性 | `InitWindow` / `InitAudioDevice` 失败未检查，后续渲染/音频调用可能崩溃或静默失效 |
| L4 | [`src/player.c`](src/player.c:36)、[`src/core/gameapp.c`](src/core/gameapp.c:16) | 健壮性 | `LoadTexture` / `LoadImage` 失败（id==0）后仍继续绘制/设图标，视觉异常或驱动警告；应显式报错并降级 |
| L5 | [`src/core/gamestack.c`](src/core/gamestack.c:176) | 健壮性 | `GameStackPush/Pop/Replace/ClearTo/RequestQuit` 等公开 API 未判空 `stack`，传 NULL 即解引用崩溃 |
| L6 | [`include/tools/timer.h`](include/tools/timer.h:17)、[`src/tools/timer.c`](src/tools/timer.c:17) | 逻辑缺陷 | `PauseTimer` 置位后无 `Resume` 接口可复位（仅 `InitTimer/ResetTimer` 间接复位），暂停后计时永久冻结；当前无调用方 |
| L7 | [`src/player.c`](src/player.c:65) | 逻辑/设计 | 起跳判定 `jumpPressed \|\| jumpHeld` 导致按住跳跃键落地瞬间无限连跳，若非设计意图需改为仅 `jumpPressed` |
| L8 | [`include/tools/animation.h`](include/tools/animation.h:32) | 逻辑缺陷 | 动画单次调用最多推进 1 帧，大 `dt`（卡顿/低帧率）时滞后于帧时间，出现短时慢放 |
| L9 | [`src/core/gamestack.c`](src/core/gamestack.c:45) | 整数溢出 | `capacity * 2` 为 int 运算，理论上溢出后导致越界写；当前规模不触发，属防御性缺陷 |
| L10 | [`include/game.h`](include/game.h:8) | 代码卫生 | 重复包含 `raylib.h`（`""` 与 `<>` 各一次），且包含大量未使用的头（`strings.h`、`math.h`），增大编译耦合 |
| L11 | [`src/tools/genrandom.c`](src/tools/genrandom.c:1)、[`include/tools/genrandom.h`](include/tools/genrandom.h:1) | 代码卫生 | 文件以 UTF-8 BOM（U+FEFF）开头，部分工具链/静态分析可能报错 |
| L12 | [`CMakeLists.txt`](CMakeLists.txt:1) | 加固缺失 | 未开启编译警告（`-Wall -Wextra -Werror`）与平台安全加固（栈保护/ASLR/NX 等），对本地游戏影响有限，建议作为工程规范补齐 |

### 3.3 明确不适用项（攻击面说明）

经全量代码审查确认，本项目**当前不存在**以下攻击面（非遗漏，而是无相应输入通道）：

- SQL 注入：无数据库、无 SQL 构造。
- 命令/脚本注入：无 `system/popen` 等外部命令调用（全量搜索确认）。
- XSS / CSRF：非 Web 程序，无 HTML/表单/Cookie 概念。
- SSRF：无任何网络请求。
- 路径遍历：资源路径全部为 `GetApplicationDirectory()` 拼固定文件名，无外部可控路径输入。
- 不安全的反序列化：无任何反序列化。
- 硬编码密钥/密码、弱加密协议：无凭据、无加密逻辑。
- 内存越界（strcpy/strcat/sprintf 类）：未使用这些危险函数；字符串操作集中在 `strings.c` 的视图型封装。
- 并发/线程安全：项目为单线程主循环，无共享状态竞争、死锁、竞态；仅需留意 raylib 音频线程回调（当前未使用自定义回调）。

---

## 四、审查方法说明

1. 通读全部 `.c` / `.h` 源码并核对头文件与实现的一致性。
2. 全量正则搜索危险函数与资源操作（`strcpy/strcat/sprintf/gets/scanf/system/popen/exec/malloc/realloc/free/fopen/fread/memcpy/memmove/rand/srand/#include` 等）。
3. 逐条核对场景栈生命周期（`onEnter/onExit/onPause/onResume`、延迟请求 flush）的资源获取/释放配对。
4. 结合 raylib 5.0 的 `LoadTexture/LoadImage/DrawTexturePro/TextFormat` 语义，核对资源与临时缓冲的生命周期。
5. 对照 `docs/game_stack_architecture.md` 设计的意图，验证实现与文档是否一致（相机、场景栈、资源策略）。

> 说明：本报告基于静态审查，未做编译运行验证；建议在修复优先级最高的 M1–M3 后，开启 `-Wall -Wextra -Werror` 与 ASan/UBSan 做一轮动态验证以覆盖潜在边界。
