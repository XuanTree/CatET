#include "platform.h"
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

// 一向上平台碰撞：仅当玩家「下落」且「水平范围与平台重叠」时判定。
// 用本帧位移反推上一帧脚底位置，做扫掠检测，避免高速下落穿透平台。
void PlayerCollision(Player *player, Platform *platform, float dt) {
  if (!player || !platform)
    return;
  // 仅当贴图已加载（尺寸有效）时参与碰撞
  if (platform->size.x <= 0.0f || platform->size.y <= 0.0f)
    return;

  // 平台碰撞矩形（size/surfaceOffset 已按 GAME_SCALE 换算为世界坐标）
  float platLeft = platform->spawnPosition.x;
  float platRight = platLeft + platform->size.x;
  // 碰撞顶面取“可见表面”而非贴图顶边：贴图顶部常有透明留白，
  // 直接用贴图顶边会让玩家脚悬在可见表面上方（视觉悬浮）。
  float platTop = platform->spawnPosition.y + platform->surfaceOffset;

  // 玩家脚底（垂直支撑点）
  float playerFeet = player->position.y + player->size.y;

  // 一向上平台：只在下落（velocity.y >= 0）时检测，从下方撞顶不成立
  if (player->velocity.y < 0.0f)
    return;

  // 水平支撑判定：使用玩家水平中心点。
  // 若用整个身体宽度判断，玩家走到平台边缘时大半身体悬空仍会被判定为着地，
  // 表现为"悬浮在空中不掉落"。改用中心点后，中心一旦离开平台范围即失去支撑。
  float playerCenterX = player->position.x + player->size.x * 0.5f;
  if (playerCenterX <= platLeft || playerCenterX >= platRight)
    return;

  // 上一帧脚底位置 = 当前脚底 - 本帧位移（近似，dt 需与玩家位移用同一帧率）
  float prevFeet = playerFeet - player->velocity.y * dt;

  // 脚底在本帧内从平台顶面上方穿过顶面 → 判定落在平台上
  if (prevFeet <= platTop && playerFeet >= platTop) {
    player->position.y = platTop - player->size.y;
    player->velocity.y = 0.0f;
    player->isOnTheGround = true;
  }
}
