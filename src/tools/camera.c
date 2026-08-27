#include "game.h"

void InitCamera2D(Camera2D *camera, Vector2 target, int logicWidth,
                  int logicHeight) {
  // 偏移固定在逻辑分辨率中心：无论目标在世界何处，都呈现在屏幕正中
  camera->offset = (Vector2){.x = logicWidth * 0.5f, .y = logicHeight * 0.5f};
  camera->target = target; // 初始目标对准目标点，避免入场时相机抽搐
  camera->rotation = 0.0f;
  camera->zoom = 1.0f;
}

void UpdateCameraCenter(Camera2D *camera, Vector2 target) {
  // 硬跟随：相机目标每帧直接钉在目标点上，目标始终位于屏幕中心
  camera->target = target;
}

void UpdateCameraSmoothFollow(Camera2D *camera, Vector2 target, float dt) {
  // 平滑跟随（参考 raylib 官网上的示例 raylib [core] example - 2d camera
  // platformer）：
  // minEffectLength：目标与相机距离小于该值视为「已对齐」，直接贴合，消除尾随抖动
  // fractionSpeed：每帧按距离的一定比例逼近，距离越远追得越快
  // minSpeed：保证最小追赶速度，避免距离稍远时相机慢得像卡住

  const float minSpeed = 30.0f;
  const float minEffectLength = 10.0f;
  const float fractionSpeed = 0.8f;

  Vector2 diff = Vector2Subtract(target, camera->target);
  float length = Vector2Length(diff);

  if (length > minEffectLength) {
    float speed = fmaxf(fractionSpeed * length, minSpeed);
    Vector2 step = Vector2Scale(diff, speed * dt / length);
    camera->target = Vector2Add(camera->target, step);
  } else {
    camera->target = target;
  }
}

void UpdateCameraEvenOutOnLanding(Camera2D *camera, Vector2 target, float dt) {
  // 垂直方向定义一条随时间下移的「目标线」，目标高于目标线时相机跟随目标，
  // 否则目标线钳制在屏幕下部，再对 y 做插值，实现跳跃/落地时画面平滑过渡。
  const float evenOutSpeed = 700.0f; // 目标线下移速度（像素/秒）
  const float eveningOut = 0.25f;    // 每帧插值比例（越大越跟手）
  const float evenOutMaxY = 0.5f;    // 目标线 y 上限（逻辑高度比例）

  // offset 固定为逻辑分辨率的一半，反推逻辑高度，让相机工具自包含
  float logicHeight = camera->offset.y * 2.0f;

  // 当前帧目标线的 y：从屏幕下部位置开始向下移动一个 dt 的距离
  float targetY = (logicHeight - evenOutMaxY * logicHeight) - evenOutSpeed * dt;

  // 目标高于目标线时目标线跟随目标；目标线不得低于 evenOutMaxY 对应位置
  if (target.y < targetY) {
    targetY = target.y;
  } else if (targetY < evenOutMaxY * logicHeight) {
    targetY = evenOutMaxY * logicHeight;
  }

  // 水平方向保持当前视角，仅对垂直目标做插值平滑
  camera->target.y = Lerp(camera->target.y, targetY, eveningOut);
}

void UpdateCameraPlayerBoundsPush(Camera2D *camera, Vector2 target, float dt) {
  // 目标在屏幕中央自由区内移动时相机保持不动，一旦触及屏幕边缘，
  // 相机被推向目标所在方向，实现「边缘触发」式跟随，视野更稳定。
  (void)dt; // 边界推动与帧间隔无关，纯位置判定

  const float bbox = 0.2f; // 触发边界（比例）：屏幕中心 1-2*bbox 范围为自由区

  // offset 固定为逻辑分辨率的一半，反推逻辑宽高
  float logicWidth = camera->offset.x * 2.0f;
  float logicHeight = camera->offset.y * 2.0f;

  // 目标越过左/右边界时，把 target 推到目标身后一个边界宽度的位置
  if (target.x < bbox * logicWidth) {
    camera->target.x = target.x - bbox * logicWidth;
  } else if (target.x > (1.0f - bbox) * logicWidth) {
    camera->target.x = target.x - (1.0f - bbox) * logicWidth;
  }

  if (target.y < bbox * logicHeight) {
    camera->target.y = target.y - bbox * logicHeight;
  } else if (target.y > (1.0f - bbox) * logicHeight) {
    camera->target.y = target.y - (1.0f - bbox) * logicHeight;
  }
}

// 场景相机组件

void InitSceneCamera(SceneCamera *sc, int logicWidth, int logicHeight,
                     bool isEnabled, CameraFollowMode mode) {
  sc->target = (Vector2){0, 0};
  InitCamera2D(&sc->camera, sc->target, logicWidth, logicHeight);
  sc->enabled = isEnabled;
  sc->mode = mode;
}

void SetCameraTarget(SceneCamera *sc, Vector2 target) { sc->target = target; }

void UpdateSceneCamera(SceneCamera *sc, float dt) {
  // 场景禁用相机时不更新，保持固定视野
  if (!sc->enabled) {
    return;
  }

  switch (sc->mode) {
  case CAMERA_FOLLOW_CENTER:
    UpdateCameraCenter(&sc->camera, sc->target);
    break;
  case CAMERA_FOLLOW_SMOOTH:
    UpdateCameraSmoothFollow(&sc->camera, sc->target, dt);
    break;
  case CAMERA_FOLLOW_EVEN_OUT_LANDING:
    UpdateCameraEvenOutOnLanding(&sc->camera, sc->target, dt);
    break;
  case CAMERA_FOLLOW_BOUNDS_PUSH:
    UpdateCameraPlayerBoundsPush(&sc->camera, sc->target, dt);
    break;
  case CAMERA_FOLLOW_NONE:
  default:
    // 不跟随：保持当前视角，仅按需绘制
    break;
  }
}

void BeginSceneCamera(SceneCamera *sc) {
  if (sc->enabled) {
    BeginMode2D(sc->camera);
  }
}

void EndSceneCamera(SceneCamera *sc) {
  if (sc->enabled) {
    EndMode2D();
  }
}
