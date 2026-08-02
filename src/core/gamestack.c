#include "core/gamestack.h"
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// GameStack 实现：
//   - scenes    : 动态数组模拟栈，栈底在下标 0，栈顶在 size-1。
//   - pending   : 延迟请求队列。公开切换 API 只入队，GameStackUpdate 帧首
//                 FlushPending 统一应用到真实栈，避免场景回调中修改栈导致
//                 use-after-free。
// ─────────────────────────────────────────────────────────────────────────────

typedef enum SceneRequestType {
  SCENE_REQUEST_NONE = 0,
  SCENE_REQUEST_PUSH,
  SCENE_REQUEST_POP,
  SCENE_REQUEST_REPLACE,
  SCENE_REQUEST_CLEAR_TO,
  SCENE_REQUEST_QUIT,
} SceneRequestType;

typedef struct SceneRequest {
  SceneRequestType type;
  GameScene *scene; // PUSH / REPLACE / CLEAR_TO 使用
} SceneRequest;

struct GameStack {
  GameScene **scenes;
  int capacity;
  int size;

  SceneRequest *pending;
  int pendingCapacity;
  int pendingSize;

  bool wantsQuit;
};

#define DEFAULT_CAPACITY 8

// ── 内部工具 ────────────────────────────────────────────────────────────────

static void EnsureSceneCapacity(GameStack *stack) {
  if (stack->size < stack->capacity)
    return;
  stack->capacity =
      (stack->capacity == 0) ? DEFAULT_CAPACITY : stack->capacity * 2;
  stack->scenes = (GameScene **)realloc(
      stack->scenes, sizeof(GameScene *) * (size_t)stack->capacity);
}

static void EnsurePendingCapacity(GameStack *stack) {
  if (stack->pendingSize < stack->pendingCapacity)
    return;
  stack->pendingCapacity = (stack->pendingCapacity == 0)
                               ? DEFAULT_CAPACITY
                               : stack->pendingCapacity * 2;
  stack->pending = (SceneRequest *)realloc(
      stack->pending, sizeof(SceneRequest) * (size_t)stack->pendingCapacity);
}

static void FreeScene(GameScene *scene) {
  if (scene->onExit)
    scene->onExit(scene);
  free(scene->data);
  free(scene);
}

// 立即压入：覆盖当前栈顶（触发其 onPause），新场景触发 onEnter
static void PushImmediate(GameStack *stack, GameScene *scene) {
  if (!scene)
    return;
  if (stack->size > 0) {
    GameScene *top = stack->scenes[stack->size - 1];
    if (top->onPause)
      top->onPause(top);
  }
  EnsureSceneCapacity(stack);
  stack->scenes[stack->size++] = scene;
  if (scene->onEnter)
    scene->onEnter(scene);
}

// 立即弹出：栈顶 onExit + free，恢复下层（触发其 onResume）
static void PopImmediate(GameStack *stack) {
  if (stack->size == 0)
    return;
  GameScene *top = stack->scenes[--stack->size];
  FreeScene(top);
  if (stack->size > 0) {
    GameScene *newTop = stack->scenes[stack->size - 1];
    if (newTop->onResume)
      newTop->onResume(newTop);
  }
}

// 立即替换栈顶：旧栈顶 onExit + free（不触发下层 onResume，因马上被覆盖），
// 新场景压入并触发 onEnter
static void ReplaceImmediate(GameStack *stack, GameScene *scene) {
  if (!scene)
    return;
  if (stack->size == 0) {
    PushImmediate(stack, scene);
    return;
  }
  GameScene *top = stack->scenes[--stack->size];
  FreeScene(top);
  EnsureSceneCapacity(stack);
  stack->scenes[stack->size++] = scene;
  if (scene->onEnter)
    scene->onEnter(scene);
}

// 立即清空到只剩指定场景：现有栈逐层 onExit + free，再压入指定场景
// （注意：传入的 scene 应为新建场景，不应是栈内已有对象）
static void ClearToImmediate(GameStack *stack, GameScene *scene) {
  if (!scene)
    return;
  for (int i = 0; i < stack->size; i++)
    FreeScene(stack->scenes[i]);
  stack->size = 0;
  EnsureSceneCapacity(stack);
  stack->scenes[stack->size++] = scene;
  if (scene->onEnter)
    scene->onEnter(scene);
}

// 将延迟请求统一应用到真实栈（帧首调用）
static void FlushPending(GameStack *stack) {
  for (int i = 0; i < stack->pendingSize; i++) {
    SceneRequest *req = &stack->pending[i];
    switch (req->type) {
    case SCENE_REQUEST_PUSH:
      PushImmediate(stack, req->scene);
      break;
    case SCENE_REQUEST_POP:
      PopImmediate(stack);
      break;
    case SCENE_REQUEST_REPLACE:
      ReplaceImmediate(stack, req->scene);
      break;
    case SCENE_REQUEST_CLEAR_TO:
      ClearToImmediate(stack, req->scene);
      break;
    case SCENE_REQUEST_QUIT:
      stack->wantsQuit = true;
      break;
    default:
      // 未知请求：释放未消费的场景，避免内存泄漏
      if (req->scene)
        free(req->scene->data), free(req->scene);
      break;
    }
  }
  stack->pendingSize = 0;
}

// ── 生命周期 ─────────────────────────────────────────────────────────────────

GameStack *GameStackCreate(void) {
  return (GameStack *)calloc(1, sizeof(GameStack));
}

void GameStackDestroy(GameStack *stack) {
  if (!stack)
    return;
  FlushPending(stack); // 先把未应用请求消费掉，避免泄漏
  for (int i = 0; i < stack->size; i++)
    FreeScene(stack->scenes[i]);
  free(stack->scenes);
  free(stack->pending);
  free(stack);
}

// ── 切换请求（延迟入队） ────────────────────────────────────────────────────

void GameStackPush(GameStack *stack, GameScene *scene) {
  EnsurePendingCapacity(stack);
  stack->pending[stack->pendingSize].type = SCENE_REQUEST_PUSH;
  stack->pending[stack->pendingSize].scene = scene;
  stack->pendingSize++;
}

void GameStackPop(GameStack *stack) {
  EnsurePendingCapacity(stack);
  stack->pending[stack->pendingSize].type = SCENE_REQUEST_POP;
  stack->pending[stack->pendingSize].scene = NULL;
  stack->pendingSize++;
}

void GameStackReplace(GameStack *stack, GameScene *scene) {
  EnsurePendingCapacity(stack);
  stack->pending[stack->pendingSize].type = SCENE_REQUEST_REPLACE;
  stack->pending[stack->pendingSize].scene = scene;
  stack->pendingSize++;
}

void GameStackClearTo(GameStack *stack, GameScene *scene) {
  EnsurePendingCapacity(stack);
  stack->pending[stack->pendingSize].type = SCENE_REQUEST_CLEAR_TO;
  stack->pending[stack->pendingSize].scene = scene;
  stack->pendingSize++;
}

void GameStackRequestQuit(GameStack *stack) {
  EnsurePendingCapacity(stack);
  stack->pending[stack->pendingSize].type = SCENE_REQUEST_QUIT;
  stack->pending[stack->pendingSize].scene = NULL;
  stack->pendingSize++;
}

// ── 查询 ─────────────────────────────────────────────────────────────────────

GameScene *GameStackTop(GameStack *stack) {
  return (stack && stack->size > 0) ? stack->scenes[stack->size - 1] : NULL;
}

int GameStackSize(GameStack *stack) { return stack ? stack->size : 0; }

bool GameStackEmpty(GameStack *stack) {
  return stack ? (stack->size == 0) : true;
}

bool GameStackWantsQuit(GameStack *stack) {
  return stack ? stack->wantsQuit : false;
}

// ── 每帧驱动 ─────────────────────────────────────────────────────────────────

void GameStackUpdate(GameStack *stack, float dt) {
  if (!stack)
    return;
  FlushPending(stack); // 帧首：应用本帧前收到的切换请求
  if (stack->wantsQuit || stack->size == 0)
    return;
  // 从栈顶到底层：仅更新活跃场景，以及标记 UPDATE_WHEN_HIDDEN 的隐藏场景
  for (int i = stack->size - 1; i >= 0; i--) {
    GameScene *s = stack->scenes[i];
    bool isTop = (i == stack->size - 1);
    bool shouldUpdate = isTop || (s->flags & GAME_SCENE_UPDATE_WHEN_HIDDEN);
    if (shouldUpdate && s->onUpdate)
      s->onUpdate(s, dt);
  }
}

void GameStackDraw(GameStack *stack) {
  if (!stack || stack->size == 0)
    return;
  // 从底层到栈顶：仅绘制活跃场景，以及标记 DRAW_WHEN_HIDDEN 的隐藏场景
  for (int i = 0; i < stack->size; i++) {
    GameScene *s = stack->scenes[i];
    bool isTop = (i == stack->size - 1);
    bool shouldDraw = isTop || (s->flags & GAME_SCENE_DRAW_WHEN_HIDDEN);
    if (shouldDraw && s->onDraw)
      s->onDraw(s);
  }
}
