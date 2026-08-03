#ifndef CAMERA_H
#define CAMERA_H

#pragma once
#include <raylib.h>
#include <raymath.h>

// 由每个场景自己持有，在InitSceneCamera函数中决定是否启用Camera

void InitCamera2D(Camera2D *camera, Vector2 target, int logicWidth,
                  int logicHeight);
void UpdateCameraCenter(Camera2D *camera, Vector2 target);
void UpdateCameraSmoothFollow(Camera2D *camera, Vector2 target, float dt);
// 落地平滑：垂直方向随目标缓动过渡，避免跳跃时画面剧烈跳动
void UpdateCameraEvenOutOnLanding(Camera2D *camera, Vector2 target, float dt);
// 边界推动：目标在屏幕自由区内移动时相机不动，触及边缘时被推动
void UpdateCameraPlayerBoundsPush(Camera2D *camera, Vector2 target, float dt);

typedef enum CameraFollowMode {
  CAMERA_FOLLOW_CENTER,           // 硬跟随：目标恒在屏幕中心
  CAMERA_FOLLOW_SMOOTH,           // 平滑跟随
  CAMERA_FOLLOW_EVEN_OUT_LANDING, // 落地平滑
  CAMERA_FOLLOW_BOUNDS_PUSH,      // 边界推动
  CAMERA_FOLLOW_NONE,             // 不跟随：保持当前视角
} CameraFollowMode;

typedef struct SceneCamera {
  Camera2D camera;
  bool enabled;
  CameraFollowMode mode;
  Vector2 target;
} SceneCamera;

void InitSceneCamera(SceneCamera *sc, int logicWidth, int logicHeight,
                     bool isEnabled, CameraFollowMode mode);
void SetCameraTarget(SceneCamera *sc, Vector2 target);
void UpdateSceneCamera(SceneCamera *sc, float dt);
void BeginSceneCamera(SceneCamera *sc);
void EndSceneCamera(SceneCamera *sc);

#endif // CAMERA_H
