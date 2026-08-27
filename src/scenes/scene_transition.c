#include "game.h"
#include <raylib.h>
#include <stdlib.h>

// ─────────────────────────────────────────────────────────────────────────────
// 通用过渡场景：两段式遮罩动画（淡入 → 保持 → 淡出），计时达到
// TRANSITION_SECONDS 后自动 Replace 到 next 目标场景。
// 三种转场用法（菜单→关卡 / 关卡→关卡 / 关卡→菜单）见 scene_transition.h。
// ─────────────────────────────────────────────────────────────────────────────

// 遮罩动画阶段在总时长中的占比（三者之和为 1）：
//   前 30% 淡入（遮罩 1 → 0）
//   中间 40% 保持（遮罩为 0）
//   后 30% 淡出（遮罩 0 → 1，为切换到目标场景做准备）
#define TRANSITION_FADE_IN_FRACTION 0.30f
#define TRANSITION_FADE_OUT_FRACTION 0.30f

// 场景私有数据：栈持有并负责释放
typedef struct TransitionData {
  const GameApp *app;    // 只读引用，不拥有
  Timer transitionTimer; // 过渡计时器（onEnter 重置，onUpdate 累计）
  GameScene *next;       // 目标场景（所有权已转移，超时 Replace 消费）
} TransitionData;

// 由已过时间 t 计算遮罩不透明度 alpha（0 = 全透，1 = 全黑）：
// 淡入段线性 1→0，保持段恒 0，淡出段线性 0→1。
static float TransitionAlpha(float t) {
  const float T = TRANSITION_SECONDS;
  const float fadeInEnd = T * TRANSITION_FADE_IN_FRACTION;
  const float fadeOutStart = T * (1.0f - TRANSITION_FADE_OUT_FRACTION);

  if (t < fadeInEnd) {
    return 1.0f - t / fadeInEnd; // 淡入：1 → 0
  } else if (t < fadeOutStart) {
    return 0.0f; // 保持
  }
  return (t - fadeOutStart) / (T - fadeOutStart); // 淡出：0 → 1
}

static void TransitionEnter(GameScene *self) {
  TransitionData *d = (TransitionData *)self->data;
  // 重置计时器：startTime = GetTime()、elapsedTime = 0、isTimerStart = false。
  // 注意：不可直接置 startTime = 0，否则 elapsedTime 会等于 GetTime()，
  // 导致首帧即判定超时、立即触发切换。
  ResetTimer(&d->transitionTimer);
}

static void TransitionUpdate(GameScene *self, float dt) {
  (void)dt; // Timer 基于 GetTime() 累计，与帧间隔无关
  TransitionData *d = (TransitionData *)self->data;
  UpdateTimer(&d->transitionTimer);

  // 计时达到阈值后自动 Replace 到目标场景。
  // 延迟请求由 GameStackUpdate 帧首统一 flush，回调内调用是安全的。
  if (GetElapsedTime(&d->transitionTimer) >= TRANSITION_SECONDS) {
    // 注意：所有权此刻转移给栈（GameStackReplace 入队的场景由帧首消费），
    // 必须先把 d->next 置 NULL，否则本场景 onExit 会误判 next 未消费而
    // 双重释放（栈持有一份 + onExit 又 free 一份 → 切换后崩溃）。
    GameScene *next = d->next;
    d->next = NULL;
    GameStackReplace(self->owner, next);
  }
}

static void TransitionDraw(GameScene *self) {
  TransitionData *d = (TransitionData *)self->data;
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;

  // 背景：浅色底，保证黑色遮罩淡入淡出有足够对比度
  DrawRectangle(0, 0, screenW, screenH, RAYWHITE);

  // 两段式黑色遮罩：淡入(1→0) → 保持(0) → 淡出(0→1)
  const float alpha = TransitionAlpha(GetElapsedTime(&d->transitionTimer));
  if (alpha > 0.0f) {
    DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, alpha));
  }
}

static void TransitionExit(GameScene *self) {
  TransitionData *d = (TransitionData *)self->data;
  // 正常路径：TransitionUpdate 已把 d->next 置 NULL（所有权转移给栈），
  // 此处为 NULL，不会释放。
  // 异常路径：本场景被直接 Pop（next 未被消费），此处释放防泄漏。
  // 注意：next 尚未压栈、onEnter 未调用，因此只释放内存、不调用 onExit。
  if (d->next != NULL) {
    free(d->next->data);
    free(d->next);
    d->next = NULL;
  }
}

GameScene *TransitionSceneCreate(const GameApp *app, GameScene *next) {
  GameScene *scene = (GameScene *)calloc(1, sizeof(GameScene));
  if (scene == NULL) {
    return NULL;
  }
  TransitionData *data = (TransitionData *)calloc(1, sizeof(TransitionData));
  if (data == NULL) {
    free(scene);
    return NULL;
  }

  data->app = app;
  data->next = next; // 转移目标场景所有权给本场景

  scene->name = "TransitionScene";
  scene->data = data;
  scene->flags = GAME_SCENE_NONE; // 全屏不透明过渡，不依赖下层绘制
  scene->pauseable = false;       // 过场不允许调出暂停画面
  scene->onEnter = TransitionEnter;
  scene->onUpdate = TransitionUpdate;
  scene->onDraw = TransitionDraw;
  scene->onExit = TransitionExit;
  // onPause / onResume 本场景不需要，保持 NULL
  return scene;
}
