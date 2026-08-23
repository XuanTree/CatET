#include "entities/platform.h"
#include "core/game_config.h"
#include "raylib.h"

// 内部实现：按 platform->platformType 加载纹理，并把贴图尺寸/顶部留白换算为
// 世界坐标（乘 GAME_SCALE）存储，与玩家 size 语义一致。仅由 InitJumpPlatforms
// 调用，不对外暴露。须在 InitWindow 之后调用，否则不能安全加载纹理。
static void LoadPlatformTexture(Platform *platform) {
  const char *path = NULL;
  switch (platform->platformType) {
  case SMALL:
    path = "%sassets/sprites/platform_1.png";
    break;
  case MEDIUM:
    path = "%sassets/sprites/platform_2.png";
    break;
  case LARGE:
    path = "%sassets/sprites/platform_3.png";
    break;
  default:
    return; // 非法类型，保持原状
  }

  // 重复加载前先释放旧纹理，避免 GPU 资源泄漏
  if (platform->platformTexture.id != 0) {
    UnloadTexture(platform->platformTexture);
  }

  // 用独立 Image 扫描顶部透明留白（扫描后立即卸载，不参与纹理生成）。
  // 纹理沿用 LoadTexture 路径（与玩家精灵一致），避免 LoadTextureFromImage
  // 在个别 raylib 版本中与 LoadTexture 的垂直朝向不一致（导致平台倒置）。
  Image scan = LoadImage(TextFormat(path, GetApplicationDirectory()));
  if (scan.data == NULL) {
    // 加载失败：清空尺寸，DrawPlatform 将画不出内容，碰撞也会跳过
    platform->size = (Vector2){0, 0};
    platform->surfaceOffset = 0.0f;
    return;
  }

  // 扫描贴图顶部“第一个存在可见像素的行”作为可见表面基准（像素单位）。
  // 平台贴图顶部常留有透明留白，若以贴图顶边为碰撞顶面，玩家脚会悬在
  // 可见表面上方。判定阈值取 alpha>=128，忽略几乎透明的抗锯齿边缘。
  float surfaceOffset = 0.0f;
  {
    Color *pixels = LoadImageColors(scan);
    if (pixels != NULL) {
      for (int y = 0; y < scan.height; y++) {
        bool rowHasContent = false;
        for (int x = 0; x < scan.width; x++) {
          if (pixels[y * scan.width + x].a >= 128) {
            rowHasContent = true;
            break;
          }
        }
        if (rowHasContent) {
          surfaceOffset = (float)y;
          break;
        }
      }
      UnloadImageColors(pixels);
    }
  }

  // 统一以“世界坐标（已按 GAME_SCALE 缩放）”存储尺寸与顶部留白：
  // 绘制与碰撞直接使用字段值，无需再乘 GAME_SCALE。
  platform->surfaceOffset = surfaceOffset * GAME_SCALE;
  platform->size = (Vector2){(float)scan.width * GAME_SCALE,
                             (float)scan.height * GAME_SCALE};
  UnloadImage(scan);

  // 纹理加载与原实现一致（与玩家同用 LoadTexture）
  platform->platformTexture =
      LoadTexture(TextFormat(path, GetApplicationDirectory()));
}

// 平台唯一初始化入口：设置类型与位置，并加载对应纹理。
void InitJumpPlatforms(Platform *platform, Vector2 spawnPosition,
                       PlatformType platformType) {
  platform->platformType = platformType;
  // 默认放置位置，调用方可按关卡设计覆盖 spawnPosition
  platform->spawnPosition = spawnPosition;
  // 由 LoadPlatformTexture 按贴图尺寸填充；失败时保持 0（绘制与碰撞均跳过）
  platform->size = (Vector2){0, 0};
  LoadPlatformTexture(platform);
}

void DrawPlatform(Platform *platform) {
  Texture2D tex = platform->platformTexture;
  // 整张贴图作为 source，dest 按平台世界坐标尺寸绘制（左上角为锚点）
  Rectangle source = {0, 0, (float)tex.width, (float)tex.height};
  Rectangle dest = {
      .x = platform->spawnPosition.x,
      .y = platform->spawnPosition.y,
      .width = platform->size.x,
      .height = platform->size.y,
  };
  DrawTexturePro(tex, source, dest, (Vector2){0, 0}, 0.0f, WHITE);
}

// 纯矩形平台碰撞：平台视为一个静态矩形，碰撞面取可见表面（忽略顶部
// 透明留白 surfaceOffset）。用玩家矩形与平台矩形做 AABB 重叠检测
// （CheckCollisionRecs），重叠时按相对位置 / 速度方向解决碰撞：
//   - 玩家从上方落到平台顶面 → 站到平台上（isOnTheGround = true）
//   - 玩家从下方顶头 → 阻止穿入平台
void PlayerCollision(Player *player, Platform *platform) {
  if (!player || !platform)
    return;
  // 仅当贴图已加载（尺寸有效）时参与碰撞
  if (platform->size.x <= 0.0f || platform->size.y <= 0.0f)
    return;

  // 平台碰撞矩形（size/surfaceOffset 已按 GAME_SCALE 换算为世界坐标）：
  // 顶面从可见表面起算，忽略贴图顶部透明留白，避免玩家脚悬空。
  const float platTop = platform->spawnPosition.y + platform->surfaceOffset;
  const Rectangle platformRect = {
      .x = platform->spawnPosition.x,
      .y = platTop,
      .width = platform->size.x,
      .height = platform->size.y - platform->surfaceOffset,
  };
  const Rectangle playerRect = {
      .x = player->position.x,
      .y = player->position.y,
      .width = player->size.x,
      .height = player->size.y,
  };

  // 纯矩形重叠检测：玩家矩形与平台矩形无重叠则不碰撞
  if (!CheckCollisionRecs(playerRect, platformRect))
    return;

  // 单向平台语义：平台只提供「上方支撑」，玩家从下方/侧面穿过一律不响应。
  // 用玩家重心相对平台顶面的位置做判定，规避两类误判：
  //   -
  //   仅看脚底：玩家从平台下方水平经过（脚底已低于顶面）时被误判落地、挤上平台；
  //   -
  //   增加顶头分支：玩家从下方起跳会撞到平台，违背「从下方可穿过」的单向设计。
  // 因此只处理「下落且重心仍在平台顶面上方」这一种情形 → 从上方落到平台。
  const float playerCenterY = player->position.y + player->size.y * 0.5f;
  if (player->velocity.y >= 0.0f && playerCenterY < platTop) {
    player->position.y = platTop - player->size.y;
    player->velocity.y = 0.0f;
    player->isOnTheGround = true;
  }
}
