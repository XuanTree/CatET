#if defined(_WIN32)
// ── Win32 最小声明（避免 <windows.h> 与 raylib 符号冲突）────────────
// 直接包含 <windows.h> 会与 raylib.h 的 Rectangle（GDI）、CloseWindow
// （User32）等符号重名冲突（编译报 redefinition）。这里仅前向声明本文件
// 需要的 user32.dll 接口，签名与 WinUser.h 一致（x64 下 __stdcall 无实际
// 影响，保留以兼容 x86）。MinGW 默认链接 user32，无需额外 -luser32。
// Windows你到底想干什么？？？你他么的；
typedef struct HWND__ *HWND; // 窗口句柄（不透明指针）
typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned long DWORD;
typedef unsigned long long ULONG_PTR; // 与指针同宽（x64: unsigned __int64）

#define VK_MENU 0x12           // Alt 键虚拟键码
#define KEYEVENTF_KEYUP 0x0002 // keybd_event 释放标志
#define SW_RESTORE 9           // ShowWindow 还原命令

BOOL __stdcall IsIconic(HWND hWnd);
BOOL __stdcall ShowWindow(HWND hWnd, int nCmdShow);
BOOL __stdcall SetForegroundWindow(HWND hWnd);
HWND __stdcall SetFocus(HWND hWnd);
void __stdcall keybd_event(BYTE bVk, BYTE bScan, DWORD dwFlags,
                           ULONG_PTR dwExtraInfo);
#endif
#include "core/gameapp.h"
#include "tools/raygui.h"
#include "tools/strings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ── 全局 UI 字体码点收集 ─────────────────────────────────────────────
// LoadFont 只生成 ASCII(32-126) 字形且基高默认 10，既无法渲染词库中文释义，
// 放大后还会严重糊化。这里从 CET4/CET6 词库收集全部出现过的码点（含中文），
// 配合 LoadFontEx 生成包含中文字形的像素字图集；同时补全 UI 常用 ASCII。
#define UI_FONT_BASE_SIZE 48 // 像素字基准字号：标题/提示常用，小字号向下缩放
#define CODEPOINT_INIT_CAPACITY 512

// 追加一个码点到动态数组（去重 + 自动扩容；内存不足时静默丢弃，降级不崩溃）。
static void AddCodepoint(int **cps, int *count, int *cap, int cp) {
  for (int i = 0; i < *count; i++) {
    if ((*cps)[i] == cp)
      return;
  }
  if (*count >= *cap) {
    int newCap = (*cap == 0) ? CODEPOINT_INIT_CAPACITY : (*cap) * 2;
    int *tmp = (int *)realloc(*cps, sizeof(int) * (size_t)newCap);
    if (!tmp)
      return;
    *cps = tmp;
    *cap = newCap;
  }
  (*cps)[(*count)++] = cp;
}

// 解析 UTF-8 文本并收集全部码点（ASCII + 中文等）。
static void CollectCodepointsFromText(int **cps, int *count, int *cap,
                                      const unsigned char *s) {
  while (*s != '\0') {
    unsigned char c = *s;
    int cp = 0, len = 1;
    if (c < 0x80) {
      cp = c;
    } else if ((c & 0xE0) == 0xC0) {
      cp = c & 0x1F;
      len = 2;
    } else if ((c & 0xF0) == 0xE0) {
      cp = c & 0x0F;
      len = 3;
    } else if ((c & 0xF8) == 0xF0) {
      cp = c & 0x07;
      len = 4;
    } else {
      s++;
      continue;
    }
    bool valid = (len > 1);
    for (int i = 1; i < len; i++) {
      if ((s[i] & 0xC0) != 0x80) {
        valid = false;
        break;
      }
      cp = (cp << 6) | (s[i] & 0x3F);
    }
    if (!valid) {
      s++;
      continue;
    }
    AddCodepoint(cps, count, cap, cp);
    s += len;
  }
}

// 整文件读入（避免跨缓冲区截断 UTF-8 序列）；返回 malloc 缓冲，调用方 free。
static unsigned char *ReadWholeFile(const char *path, size_t *outSize) {
  *outSize = 0;
  FILE *fp = fopen(path, "rb");
  if (!fp)
    return NULL;
  fseek(fp, 0, SEEK_END);
  long sz = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  if (sz <= 0) {
    fclose(fp);
    return NULL;
  }
  unsigned char *buf = (unsigned char *)malloc((size_t)sz + 1);
  if (!buf) {
    fclose(fp);
    return NULL;
  }
  size_t rd = fread(buf, 1, (size_t)sz, fp);
  fclose(fp);
  buf[rd] = '\0';
  *outSize = rd;
  return buf;
}

// 收集词库全部码点：先补全 UI 必需 ASCII(32-126)，再合并 CET4/CET6 出现字符。
// 返回 malloc 数组（调用方 free），*outCount 记录数量。
static int *CollectWordBankCodepoints(const char *appDir, int *outCount) {
  int cap = 0;
  int *cps = NULL;
  *outCount = 0;
  for (int cp = 32; cp <= 126; cp++)
    AddCodepoint(&cps, outCount, &cap, cp);

  const char *names[] = {"CET4.txt", "CET6.txt"};
  for (int f = 0; f < 2; f++) {
    // 拼接路径：String 自动扩容，规避固定缓冲区对 appDir 长度的假设
    String path = StringCreate(appDir);
    StringAppend(&path, "assets/words/");
    StringAppend(&path, names[f]);
    size_t size = 0;
    unsigned char *data = ReadWholeFile(StringData(&path), &size);
    StringFree(&path);
    if (data) {
      CollectCodepointsFromText(&cps, outCount, &cap, data);
      free(data);
    }
  }
  return cps;
}

GameApp GameAppInit(const int logicWidth, const int logicHeight,
                    const char *title) {
  GameApp app = {0};
  app.logicWidth = logicWidth;
  app.logicHeight = logicHeight;

  // 允许窗口自由缩放
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(logicWidth, logicHeight, title);
  // 禁用 ESC 默认关闭窗口：ESC 交给主循环用于弹出暂停界面
  SetExitKey(0);
  // 游戏全局隐藏鼠标：开始/暂停/关卡界面均已支持纯键盘操作
  HideCursor();
  // 限制最小窗口尺寸，避免被压缩得过小
  SetWindowMinSize(logicWidth, logicHeight);

  // 窗口图标
  Image icon = LoadImage(
      TextFormat("%sassets/sprites/icon.png", GetApplicationDirectory()));
  SetWindowIcon(icon);
  app.icon = icon; // 保留以便最后卸载

  // 固定分辨率渲染目标：内部始终按逻辑分辨率渲染，防止放大后画面模糊
  app.target = LoadRenderTexture(logicWidth, logicHeight);
  // 双线性过滤，让缩放后的画面保持平滑清晰
  SetTextureFilter(app.target.texture, TEXTURE_FILTER_BILINEAR);

  app.isPaused = false;
  SetTargetFPS(60);
  InitAudioDevice();

  // UI 音效：加载失败时置 uiSoundValid=false，各场景播放前据此静默跳过。
  app.uiSound = LoadSound(
      TextFormat("%sassets/sounds/ui_sound.ogg", GetApplicationDirectory()));
  app.uiSoundValid = IsSoundValid(app.uiSound);

  // 全局 UI 字体：用 LoadFontEx 生成包含词库中文码点的像素字图集，
  // 供界面与中文释义共用；失败降级到默认字体。像素字体用点采样保持锐利。
  int cpCount = 0;
  int *codepoints =
      CollectWordBankCodepoints(GetApplicationDirectory(), &cpCount);
  Font uiFont = {0};
  if (codepoints && cpCount > 0) {
    uiFont = LoadFontEx(
        TextFormat("%sassets/fonts/pixel_font.ttf", GetApplicationDirectory()),
        UI_FONT_BASE_SIZE, codepoints, cpCount);
  }
  free(codepoints);
  if (IsFontValid(uiFont) && uiFont.glyphCount > 0) {
    SetTextureFilter(uiFont.texture, TEXTURE_FILTER_POINT);
    app.uiFont = uiFont;
    app.uiFontLoaded = true;
    // raygui 控件同样使用像素字体，并调大默认字号（原 10 在 640x480 下过小）
    GuiSetFont(uiFont);
    GuiSetStyle(DEFAULT, TEXT_SIZE, 20);
  } else {
    app.uiFont = GetFontDefault();
    app.uiFontLoaded = false;
  }

  return app;
}

// 计算逻辑分辨率到窗口的等比缩放（含居中黑边）
// 逻辑分辨率：场景以固定逻辑分辨率绘制，raygui 的按钮矩形也位于该空间，
// 若直接用实际窗口像素坐标做命中检测，窗口放大后交互就会错位。
// raylib 的鼠标变换为 L = M*scale' + offset'，要得到 L = (M - 黑边)/scale，
// 但是其实我也不想让这游戏有什么鼠标操作。。。呃？
// 因此 scale' = 1/scale，offset' = -黑边/scale。
static float ApplyViewportScale(GameApp *app) {
  float screenW = (float)GetScreenWidth();
  float screenH = (float)GetScreenHeight();
  float scale = fminf(screenW / (float)app->logicWidth,
                      screenH / (float)app->logicHeight);
  float offsetX = (screenW - (float)app->logicWidth * scale) * 0.5f;
  float offsetY = (screenH - (float)app->logicHeight * scale) * 0.5f;

  SetMouseScale(1.0f / scale, 1.0f / scale);
  SetMouseOffset((int)roundf(-offsetX / scale), (int)roundf(-offsetY / scale));
  return scale;
}

void GameAppBegin(GameApp *app) {
  // 先同步鼠标到逻辑坐标，保证本帧 GUI（raygui）命中检测准确
  ApplyViewportScale(app);
  BeginTextureMode(app->target);
  ClearBackground(RAYWHITE);
}

void GameAppEnd(GameApp *app) {
  (void)app;
  EndTextureMode();
}

void GameAppPresent(GameApp *app) {
  BeginDrawing();
  ClearBackground(BLACK);

  float screenW = (float)GetScreenWidth();
  float screenH = (float)GetScreenHeight();
  float scale = ApplyViewportScale(app);
  float destW = (float)app->logicWidth * scale;
  float destH = (float)app->logicHeight * scale;
  Rectangle dest = {
      .x = (screenW - destW) * 0.5f,
      .y = (screenH - destH) * 0.5f,
      .width = destW,
      .height = destH,
  };
  // RenderTexture 的纹理在 OpenGL 中上下颠倒，source 高度取负
  Rectangle sourceTex = {0, 0, (float)app->logicWidth,
                         -(float)app->logicHeight};
  DrawTexturePro(app->target.texture, sourceTex, dest, (Vector2){0, 0}, 0.0f,
                 WHITE);

  EndDrawing();
}

// 全屏切换后需要强制聚焦的剩余帧数。Windows 异步处理窗口样式/尺寸切换
// （SetWindowLongPtr + SetWindowPos），切换瞬间调用聚焦 API 会被随后到达的
// 窗口消息覆盖，因此需要跨越足够长的帧数持续重试。
static int s_refocusFrames = 0;
// 当前是否处于无边框全屏模式（由 ToggleBorderlessWindowed 切换）
static bool s_borderless = false;
// 切换后强制聚焦的持续帧数：约 2 秒（60 FPS），覆盖异步切换的耗时
#define REFOCUS_FRAMES 120

// 强制窗口获得键盘焦点。
// 仅调用 raylib 的 SetWindowFocused 在 Windows 前台锁（Foreground Lock）下
// 并不可靠：当进程已不在前台时，系统会拒绝其 SetForegroundWindow 调用，焦点
// 抢不回来，键盘输入（W/S/↑↓/Z/X）持续失效。这里先用 keybd_event 模拟一次
// 按键事件（释放 Alt），让系统认为本进程"刚刚收到用户输入"从而解锁前台限制，
// 再 SetForegroundWindow 强制抢回焦点，最后 SetFocus 确保键盘消息送达本窗口。
// 这是 Windows 游戏维持焦点的通用做法。非 Windows 平台回退到 raylib 通用实现。
static void ForceWindowFocus(void) {
#if defined(_WIN32)
  HWND hwnd = (HWND)GetWindowHandle();
  if (hwnd != 0) {
    if (IsIconic(hwnd)) {
      ShowWindow(hwnd, SW_RESTORE); // 最小化时先还原，否则聚焦 API 无效
    }
    keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0); // 解锁 Windows 前台锁
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    return;
  }
#endif
  SetWindowFocused();
}

void GameAppPollGlobalInput(void) {
  // F11 或 Alt+Enter 切换全屏
  if (IsKeyPressed(KEY_F11) ||
      (IsKeyDown(KEY_LEFT_ALT) && IsKeyPressed(KEY_ENTER))) {
    // 使用无边框窗口化全屏（ToggleBorderlessWindowed）替代独占全屏
    // （ToggleFullscreen）。Windows 独占全屏通过 ChangeDisplaySettings
    // (CDS_FULLSCREEN) 切换显示模式，切换瞬间会触发 WM_KILLFOCUS 使窗口失去
    // 键盘焦点，之后 IsKeyPressed/IsKeyDown 收不到任何输入，UI 的键盘操控
    // （W/S/↑↓/Z/X）就表现为"卡住"。无边框全屏只调整窗口尺寸/位置、不切换
    // 显示模式，从根源规避该问题。
    ToggleBorderlessWindowed();
    s_borderless = !s_borderless;
    s_refocusFrames = REFOCUS_FRAMES; // 切换后持续强制聚焦以覆盖异步处理
  }

  // 无边框全屏期间持续确保窗口聚焦，避免失焦导致键盘输入失效；
  // 切换后的延迟帧内无条件强制聚焦，弥补切换瞬间窗口未就绪的情况。
  if ((s_borderless && !IsWindowFocused()) || (s_refocusFrames > 0)) {
    ForceWindowFocus();
    if (s_refocusFrames > 0)
      s_refocusFrames--;
  }
}

void GameAppClose(GameApp *app) {
  UnloadRenderTexture(app->target);
  UnloadImage(app->icon);
  UnloadSound(app->uiSound);
  // 仅卸载真正加载的自定义字体（降级用的默认字体归 raylib 内部管理）
  if (app->uiFontLoaded) {
    UnloadFont(app->uiFont);
  }
  CloseAudioDevice();
  CloseWindow();
}

void GameAppPaused(GameApp *app) {
  if (IsKeyDown(KEY_ESCAPE)) {
    app->isPaused = true;
  }
}

void GameAppResume(GameApp *app) {
  if (IsKeyDown(KEY_ESCAPE)) {
    app->isPaused = false;
  }
}

// 使用全局像素字体绘制文本（等价于 DrawText，但应用 uiFont，支持中文释义）。
// 字间距取字号 1/10，与 maze 关卡 HUD 的 DrawTextEx 用法保持一致。
void GameAppDrawText(const GameApp *app, const char *text, int posX, int posY,
                     int fontSize, Color color) {
  if (!app || !text)
    return;
  DrawTextEx(app->uiFont, text, (Vector2){(float)posX, (float)posY},
             (float)fontSize, (float)fontSize / 10.0f, color);
}

// 使用全局像素字体测量文本宽度（等价于 MeasureText）。
int GameAppMeasureText(const GameApp *app, const char *text, int fontSize) {
  if (!app || !text)
    return 0;
  return (int)MeasureTextEx(app->uiFont, text, (float)fontSize,
                            (float)fontSize / 10.0f)
      .x;
}
