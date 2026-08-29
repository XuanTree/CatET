#include "game.h"
#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// 迷宫解密关卡（docs/game_instructions.md 关卡设计 2，重做为侧视平台跳跃式）：
//   - 纵向迷宫：以 40px 网格砌出封闭的迷宫箱（侧视如蚂蚁地穴，无需出口），
//     四周为实心边界墙，内部用随机化 Prim 算法在「房间网格」中生长出随机
//     连通的分支隧道网络（腔室 + 竖井），带死胡同与环路，每局布局不同。
//   - 玩家使用平台跳跃物理（重力 + 跳跃）在隧道间穿梭寻找字母。
//   - 中央设有一块「拼写平台」（绿色高亮），玩家把找到的字母带回此处放下拼写。
//   - 拼写平台以外任意位置均可放下字母（可随时放弃/更换，避免误扣血）。
//   - 拼写正确进入下一关；拼写错误扣血并重置；HP 归零失败。
//   - 跟随镜头（纵向为主）。
// 感谢Deepseek对于迷宫生成算法的贡献
// ─────────────────────────────────────────────────────────────────────────────

#define MAZE_TIME_LIMIT 60.0f // Update: 迷宫关卡限时60（秒）
#define WRONG_PENALTY 20.0f   // 拼写错误扣血
#define TIME_PENALTY 20.0f    // 倒计时归零扣血（并重置倒计时）
#define LETTER_RADIUS 22.0f   // 字母拾取半径
#define MAZE_MAX_LETTERS 8
#define DISTRACTOR_COUNT 4 // 干扰字母数量（正确字母 1 + 干扰 4）

// ── 迷宫世界布局（网格化封闭迷宫，侧视如蚂蚁地穴）───────────────────────────
// 以 40px 网格砌出封闭的纵向迷宫：四周为实心边界墙，内部为
// 实心泥土挖出的蜿蜒隧道与竖井，中段设拼写平台。世界尺寸大于屏幕（1280×1760），
// 镜头跟随玩家，迷宫可超出屏幕外而不受影响。
#define MAZE_TILE 40.0f // 网格单元边长
#define MAZE_COLS 32    // 网格列数（32×40 = 1280 宽）
#define MAZE_ROWS 44    // 网格行数（44×40 = 1760 高）
#define MAZE_WORLD_W (MAZE_COLS * MAZE_TILE)
#define MAZE_WORLD_H (MAZE_ROWS * MAZE_TILE)
#define MAZE_GROUND_Y ((MAZE_ROWS - 1) * MAZE_TILE) // 地面顶面 y=1400
#define MAZE_MAX_LEVELS 10 // 迷宫画廊层数（房间网格 5 列 × 10 层）
#define MAZE_MAX_WALLS 768 // 墙体矩形数组上限

// 场景私有数据：栈持有并负责释放
typedef struct MazeData {
  const GameApp *app; // 只读引用，不拥有
  Player cat;         // 平台跳跃玩家
  Rectangle source;   // 当前动画帧源矩形
  SceneCamera camera; // 跟随镜头
  // 迷宫：实心网格 + 合并后的墙体矩形（四周封闭，构成蚂蚁地穴式隧道）
  bool solid[MAZE_COLS][MAZE_ROWS]; // true=实心泥土
  Rectangle walls[MAZE_MAX_WALLS];  // 由实心网格合并的墙体矩形（碰撞/绘制）
  int wallCount;
  int spawnRc;                                // 出生房间列（底部一层 rl=0）
  Vector2 letterSpots[MAZE_MAX_LETTERS];      // 字母候选落点（迷宫各房间）
  bool letterSpotIsDeadEnd[MAZE_MAX_LETTERS]; // 对应落点所在房间是否为死胡同
  int letterSpotCount;
  float timeLeft;   // 关卡倒计时（秒）
  int difficulty;   // 难度（传给下一关）
  int level;        // 当前关卡编号（创建时注入，通关后经 level_flow 推进）
  GameStack *owner; // 所属栈（MazeEnter 捕获 self->owner，供拼写事件切换场景）
  // 可复用的字母拾取 + 拼写检查组件（词库、谜题、字母实体、交互状态均在其中）
  Character character;
} MazeData;

// ── 迷宫布局（随机化 Prim 生成大型封闭式蚂蚁地穴迷宫）──────────────────────
// 以 40px 网格砌出封闭迷宫箱（四周实心边界墙，无需出口），内部用随机化
// Prim 算法在「房间网格」（5 列 × 10 层，每房间 6×3 格 = 240×120px）中
// 生长一个随机连通的分支隧道网络。世界尺寸大于屏幕，镜头跟随玩家。
//   - 房间水平相邻即连通；垂直间以「居中 2 格宽竖井」连通（楼板间隔 4 行
//     160px，可跳跃；竖井两侧各留 2 格实体地面，任何房间都有可站立之处，
//     从源头杜绝「随机堵死通往拼写平台道路」的死局）；
//   - 每局生长约 45%~70% 的房间，形成随机、复杂、带死胡同与环路的分支
//     网络：已挖房间即“地穴腔室”，未挖部分即泥土隔墙；
//   - 中央设一处 12 格宽的拼写大厅（绿色高亮拼写平台）；
//   - 出生房间位于底部一层。Prim 阶段结束后统一做连通性保障（补缺失竖井 +
//     桥接出生房间与拼写大厅），保证出生房间、拼写大厅与全部字母落点同属
//     一个连通分量、必然可达，杜绝「随机堵死通往拼写平台道路」的死局。
#define MAZE_ROOM_COLS 5
#define MAZE_ROOM_LEVELS MAZE_MAX_LEVELS
#define MAZE_ROOM_TOTAL (MAZE_ROOM_COLS * MAZE_ROOM_LEVELS)
#define MAZE_ROOM_W 6 // 房间宽（格，240px）

typedef struct {
  int rc, rl;   // 已访问房间
  int drc, drl; // 邻接方向
} MazeEdge;

// 在网格中挖空一个矩形区域（用于挖房间/走廊/竖井/大厅）
static void CarveRect(MazeData *d, int x0, int y0, int w, int h) {
  for (int x = x0; x < x0 + w && x < MAZE_COLS; x++)
    for (int y = y0; y < y0 + h && y < MAZE_ROWS; y++)
      if (x >= 0 && y >= 0)
        d->solid[x][y] = false;
}

// 房间（rc, rl）占 6×3 格：列 1+6*rc..6+6*rc，行 40-4*rl..42-4*rl；
// 楼板（下方）行 = 43-4*rl。
static void CarveRoom(MazeData *d, int rc, int rl) {
  CarveRect(d, 1 + rc * MAZE_ROOM_W, 40 - 4 * rl, MAZE_ROOM_W, 3);
}

// 房间 rl 与 rl+1 之间的竖井：居中 2 格宽，穿过楼板行 39-4*rl，
// 两侧各留 2 格实体供玩家站立（保证可达、不堵死）。
static void CarveShaft(MazeData *d, int rc, int lowerRl) {
  CarveRect(d, 3 + rc * MAZE_ROOM_W, 39 - 4 * lowerRl, 2, 1);
}

// 把房间的 4 邻域边加入 Prim 前沿；allowDown=false 时禁止向下连接（用于拼写
// 大厅，避免竖井穿透其地面、破坏拼写平台）。
static void PushFrontier(MazeEdge *frontier, int *count, int rc, int rl,
                         bool allowDown) {
  static const int dcols[4] = {1, -1, 0, 0};
  static const int drows[4] = {0, 0, 1, -1};
  for (int dir = 0; dir < 4; dir++) {
    if (!allowDown && drows[dir] == -1) // drows==-1 表示向下（rl 减小为更低层）
      continue;
    int nrc = rc + dcols[dir];
    int nrl = rl + drows[dir];
    if (nrc < 0 || nrc >= MAZE_ROOM_COLS || nrl < 0 || nrl >= MAZE_ROOM_LEVELS)
      continue;
    frontier[(*count)++] = (MazeEdge){rc, rl, dcols[dir], drows[dir]};
  }
}

// 房间在图中的相邻（已挖）房间数
static int RoomDegree(const bool carved[MAZE_ROOM_COLS][MAZE_ROOM_LEVELS],
                      int rc, int rl) {
  static const int dcols[4] = {1, -1, 0, 0};
  static const int drows[4] = {0, 0, 1, -1};
  int deg = 0;
  for (int dir = 0; dir < 4; dir++) {
    int nrc = rc + dcols[dir];
    int nrl = rl + drows[dir];
    if (nrc >= 0 && nrc < MAZE_ROOM_COLS && nrl >= 0 &&
        nrl < MAZE_ROOM_LEVELS && carved[nrc][nrl])
      deg++;
  }
  return deg;
}

// 房间地面上的字母落点：置于竖井两侧的实体边缘上（永不悬空）
static Vector2 RoomSpot(int rc, int rl) {
  float x = (genRandomNum(2) ? 220.0f : 60.0f) +
            (float)(rc * MAZE_ROOM_W) * MAZE_TILE;
  return (Vector2){x, ((float)(43 - 4 * rl)) * MAZE_TILE - 8.0f};
}

// ── 连通性保障（杜绝随机“堵死”）───────────────────────────────────────────
// 原生成算法把「拼写大厅」与「出生房间」作为两个独立种子加入 Prim 前沿，
// 若在达到目标房间数前两棵分量未合并，出生房间与拼写大厅会分属不同连通
// 分量（实测约 30% 概率），玩家将无法把字母带到拼写平台。此外，两个垂直
// 相邻但未通过竖井连接、只靠水平边分别连入网络的已挖房间，会在「房间图」
// 上看似相邻、网格却仍被实心楼板隔断。以下三组函数在布局阶段结束后统一
// 修复这两类问题，保证：出生房间、拼写大厅与全部字母落点同属一个连通分量。

// 房间图 BFS：从 (startRc,startRl) 出发沿已挖房间统计可达房间并写入 out，
// 返回拼写大厅两个房间是否都可达。排除「拼写大厅正下方的幻影邻接」
// （rc=2/3 且上方房间为大厅）：该竖井因保护拼写平台而故意不挖，网格实际
// 不连通，故房间图必须与之保持一致，否则会误判大厅可达、漏掉桥接修复。
static bool
MazeRoomReachable(const bool carved[MAZE_ROOM_COLS][MAZE_ROOM_LEVELS],
                  int startRc, int startRl, int hubLevel,
                  bool out[MAZE_ROOM_COLS][MAZE_ROOM_LEVELS]) {
  static const int dcols[4] = {1, -1, 0, 0};
  static const int drows[4] = {0, 0, 1, -1};
  bool visited[MAZE_ROOM_COLS][MAZE_ROOM_LEVELS] = {false};
  int qRc[MAZE_ROOM_TOTAL], qRl[MAZE_ROOM_TOTAL];
  int head = 0, tail = 0;
  qRc[tail] = startRc;
  qRl[tail] = startRl;
  tail++;
  visited[startRc][startRl] = true;
  while (head < tail) {
    int rc = qRc[head], rl = qRl[head];
    head++;
    for (int dir = 0; dir < 4; dir++) {
      int nrc = rc + dcols[dir];
      int nrl = rl + drows[dir];
      if (nrc < 0 || nrc >= MAZE_ROOM_COLS || nrl < 0 ||
          nrl >= MAZE_ROOM_LEVELS)
        continue;
      if (!carved[nrc][nrl] || visited[nrc][nrl])
        continue;
      // 竖井方向（列相同）且上方房间是拼写大厅 → 幻影邻接，不连通
      if (nrc == rc && (nrc == 2 || nrc == 3)) {
        int lower = (rl < nrl) ? rl : nrl;
        int upper = (rl < nrl) ? nrl : rl;
        if (upper == hubLevel && lower == hubLevel - 1)
          continue;
      }
      visited[nrc][nrl] = true;
      qRc[tail] = nrc;
      qRl[tail] = nrl;
      tail++;
    }
  }
  for (int rc = 0; rc < MAZE_ROOM_COLS; rc++)
    for (int rl = 0; rl < MAZE_ROOM_LEVELS; rl++)
      out[rc][rl] = visited[rc][rl];
  return visited[2][hubLevel] && visited[3][hubLevel];
}

// 补齐所有垂直相邻且都已挖开的房间之间的竖井，使网格连通性与房间邻接一致
// （房间图认为相邻，网格就必须有竖井连通）。跳过拼写大厅正下方的竖井
// （上方房间为大厅），避免挖穿拼写平台。
static void
CarveMissingShafts(MazeData *d,
                   const bool carved[MAZE_ROOM_COLS][MAZE_ROOM_LEVELS],
                   int hubLevel) {
  for (int rc = 0; rc < MAZE_ROOM_COLS; rc++) {
    for (int rl = 0; rl < MAZE_ROOM_LEVELS - 1; rl++) {
      if (!carved[rc][rl] || !carved[rc][rl + 1])
        continue;
      bool upperIsHub = (rc == 2 || rc == 3) && (rl + 1 == hubLevel);
      if (upperIsHub)
        continue;
      CarveShaft(d, rc, rl);
    }
  }
}

// 若出生房间与拼写大厅分属不同连通分量，沿桥接列（1 或 4，避开拼写平台
// 正下方 rc=2/3）打通一条确定路径：出生层 rl=0 水平段 + 整列房间与竖井 +
// 大厅同层的水平相邻房间，使两者必然连通。
static void RepairSpawnHubBridge(MazeData *d,
                                 bool carved[MAZE_ROOM_COLS][MAZE_ROOM_LEVELS],
                                 int hubLevel, int spawnRc) {
  bool reachable[MAZE_ROOM_COLS][MAZE_ROOM_LEVELS];
  if (MazeRoomReachable(carved, spawnRc, 0, hubLevel, reachable))
    return;                               // 已连通，无需修复
  int bridgeCol = (spawnRc <= 2) ? 1 : 4; // 靠近出生房间方向的桥接列
  for (int rl = 0; rl <= hubLevel; rl++) {
    CarveRoom(d, bridgeCol, rl);
    carved[bridgeCol][rl] = true;
  }
  for (int rl = 0; rl < hubLevel; rl++)
    CarveShaft(d, bridgeCol, rl);
  int lo = (bridgeCol < spawnRc) ? bridgeCol : spawnRc;
  int hi = (bridgeCol < spawnRc) ? spawnRc : bridgeCol;
  for (int rc = lo; rc <= hi; rc++) {
    CarveRoom(d, rc, 0);
    carved[rc][0] = true;
  }
}

static void BuildMazeLayout(MazeData *d) {
  // 1) 初始全实心（四周边界墙 + 内部泥土）
  for (int x = 0; x < MAZE_COLS; x++)
    for (int y = 0; y < MAZE_ROWS; y++)
      d->solid[x][y] = true;

  bool carved[MAZE_ROOM_COLS][MAZE_ROOM_LEVELS] = {false};
  MazeEdge frontier[MAZE_ROOM_TOTAL * 4];
  int frontierCount = 0;

  // 2) 中央拼写大厅（hub）：2 个房间列合并成 12 格宽的大厅
  int hubLevel = 4 + genRandomNum(2); // 4 或 5
  CarveRect(d, 1 + 2 * MAZE_ROOM_W, 40 - 4 * hubLevel, 2 * MAZE_ROOM_W, 3);
  carved[2][hubLevel] = true;
  carved[3][hubLevel] = true;
  int hubFloorRow = 43 - 4 * hubLevel; // 拼写大厅下方楼板行
  d->character.wordPlatform =
      (Rectangle){((float)(1 + 2 * MAZE_ROOM_W)) * MAZE_TILE,
                  (float)hubFloorRow * MAZE_TILE,
                  2.0f * MAZE_ROOM_W * MAZE_TILE, MAZE_TILE};
  PushFrontier(frontier, &frontierCount, 2, hubLevel, false);
  PushFrontier(frontier, &frontierCount, 3, hubLevel, false);

  // 3) 出生房间：底部一层，随机列
  d->spawnRc = genRandomNum(MAZE_ROOM_COLS);
  CarveRoom(d, d->spawnRc, 0);
  carved[d->spawnRc][0] = true;
  PushFrontier(frontier, &frontierCount, d->spawnRc, 0, true);

  // 4) 随机化 Prim：生长连通分支隧道（约 45%~70% 房间）
  int total = MAZE_ROOM_TOTAL;
  int target =
      (int)(total * (0.45f + 0.25f * (float)genRandomNum(101) / 100.0f));
  if (target < 24)
    target = 24;
  if (target > total)
    target = total;
  int carvedCount = 3; // hub 两房间 + 出生房间

  while (frontierCount > 0 && carvedCount < target) {
    int idx = genRandomNum(frontierCount);
    MazeEdge e = frontier[idx];
    frontier[idx] = frontier[--frontierCount];
    int nrc = e.rc + e.drc;
    int nrl = e.rl + e.drl;
    if (nrc < 0 || nrc >= MAZE_ROOM_COLS || nrl < 0 || nrl >= MAZE_ROOM_LEVELS)
      continue;
    if (carved[nrc][nrl])
      continue;
    if (e.drc == 0) {
      // 垂直：挖居中竖井穿过楼板（水平相邻房间自动连通）
      int lowerRl = (e.drl > 0) ? e.rl : nrl;
      CarveShaft(d, nrc, lowerRl);
    }
    CarveRoom(d, nrc, nrl);
    carved[nrc][nrl] = true;
    carvedCount++;
    PushFrontier(frontier, &frontierCount, nrc, nrl, true);
  }

  // 4.5) 连通性保障：先补齐缺失竖井，再桥接出生房间与拼写大厅，
  //      杜绝“随机堵死通往拼写平台道路”的死局。
  CarveMissingShafts(d, carved, hubLevel);
  RepairSpawnHubBridge(d, carved, hubLevel, d->spawnRc);

  // 5) 收集字母落点：优先死胡同房间（仅 1 个相邻房间），再普通房间
  d->letterSpotCount = 0;
  for (int pass = 0; pass < 2 && d->letterSpotCount < MAZE_MAX_LETTERS;
       pass++) {
    for (int rc = 0;
         rc < MAZE_ROOM_COLS && d->letterSpotCount < MAZE_MAX_LETTERS; rc++)
      for (int rl = 0;
           rl < MAZE_ROOM_LEVELS && d->letterSpotCount < MAZE_MAX_LETTERS;
           rl++) {
        if (!carved[rc][rl])
          continue;
        if ((rc == 2 || rc == 3) && rl == hubLevel)
          continue; // 跳过拼写大厅
        int deg = RoomDegree(carved, rc, rl);
        if (pass == 0 && deg != 1)
          continue; // 第一遍只要死胡同
        if (pass == 1 && deg <= 1)
          continue; // 第二遍只要普通房间
        d->letterSpots[d->letterSpotCount] = RoomSpot(rc, rl);
        d->letterSpotIsDeadEnd[d->letterSpotCount] = (deg == 1);
        d->letterSpotCount++;
      }
  }

  // 6) 把实心网格合并为墙体矩形（每行连续实心段 = 一个矩形，供碰撞与绘制）
  d->wallCount = 0;
  for (int y = 0; y < MAZE_ROWS && d->wallCount < MAZE_MAX_WALLS; y++) {
    int x = 0;
    while (x < MAZE_COLS) {
      if (d->solid[x][y]) {
        int x2 = x;
        while (x2 < MAZE_COLS && d->solid[x2][y])
          x2++;
        d->walls[d->wallCount++] =
            (Rectangle){(float)(x * MAZE_TILE), (float)(y * MAZE_TILE),
                        (float)((x2 - x) * MAZE_TILE), MAZE_TILE};
        x = x2;
      } else {
        x++;
      }
    }
  }
}

// 收集字母候选落点：返回 BuildMazeLayout 阶段记录的迷宫房间落点
static int CollectLetterSpots(const MazeData *d, Vector2 *out, int maxOut) {
  int count = 0;
  for (int i = 0; i < d->letterSpotCount && count < maxOut; i++)
    out[count++] = d->letterSpots[i];
  return count;
}

// ── 工具函数 ─────────────────────────────────────────────────────────────────

static Rectangle PlayerRect(const Player *p) {
  return (Rectangle){p->position.x, p->position.y, p->size.x, p->size.y};
}

// 字母放下落点解析与拼写事件的实现位于本文件下方（墙体定义之后），
// 此处先声明，供 MazeEnter 注入 Character 组件回调。
static Vector2 MazeDropResolver(void *ctx, const Player *p);
static void MazeOnSpellCorrect(void *ctx);
static void MazeOnSpellWrong(void *ctx);

// ── 生命周期回调 ────────────────────────────────────────────────────────────

static void MazeEnter(GameScene *self) {
  MazeData *d = (MazeData *)self->data;

  d->cat = (Player){0};
  InitPlayer(&d->cat);
  d->cat.app = d->app; // 注入音频宿主（受伤/跳跃音效）
  // 生命值继承：进入新关卡时恢复上一关剩余 HP（新游戏 playerHealth=0 → 满血）
  if (d->app->playerHealth > 0.0f)
    d->cat.health = d->app->playerHealth;
  d->cat.lastHealth = d->cat.health; // 同步受伤检测基准，避免进场误触发

  // 初始化字母拼写组件并注入场景回调（落点解析 + 拼写事件）
  d->owner = self->owner;
  CharacterInit(&d->character);
  d->character.app = d->app; // 注入音频宿主（拾取字母音效）
  d->character.dropResolver = MazeDropResolver;
  d->character.dropCtx = d;
  d->character.onSpellCorrect = MazeOnSpellCorrect;
  d->character.onSpellWrong = MazeOnSpellWrong;
  d->character.eventCtx = d;

  // 按难度加载词库（词库加载/谜题生成/字母交互均在 Character 组件内）
  const char *relPath;
  switch (d->difficulty) {
  case 2:
    relPath = "assets/words/CET6.txt";
    break;
  case 1:
    relPath = "assets/words/CET4.txt";
    break; // 普通暂用 CET4
  case 0:
  default:
    relPath = "assets/words/CET4.txt";
    break;
  }
  CharacterLoadBankEmbedded(&d->character, relPath);

  // 构建封闭式蚂蚁地穴迷宫（同时确定拼写平台与字母候选落点）
  BuildMazeLayout(d);

  // 玩家出生在底部出生房间（地面之上）
  d->cat.position = (Vector2){60.0f + 240.0f * (float)d->spawnRc,
                              MAZE_GROUND_Y - d->cat.size.y};
  d->cat.velocity = (Vector2){0, 0};
  d->cat.isOnTheGround = true;

  // 跟随镜头
  InitSceneCamera(&d->camera, d->app->logicWidth, d->app->logicHeight, true,
                  CAMERA_FOLLOW_CENTER);

  // 抽词 + 挖空 + 放字母（正确字母 + 干扰字母散落迷宫房间）
  CharacterSetupPuzzle(&d->character);
  Vector2 spots[MAZE_MAX_LETTERS];
  bool spotIsDeadEnd[MAZE_MAX_LETTERS];
  int spotCount = CollectLetterSpots(d, spots, MAZE_MAX_LETTERS);
  for (int i = 0; i < spotCount; i++)
    spotIsDeadEnd[i] = d->letterSpotIsDeadEnd[i];
  CharacterPlaceLetters(&d->character, spots, spotIsDeadEnd, spotCount,
                        DISTRACTOR_COUNT);
  d->timeLeft = MAZE_TIME_LIMIT;

  // 隐式全局计时器：进入第一关开始计时（后续关卡保持累计）
  if (d->level == 1)
    SpeedrunStart((GameApp *)d->app);
}

// 实心墙体碰撞：最小穿透深度 AABB 分离解决（可站立、顶头、贴墙滑动）。
// 墙体在四周均阻挡，构成封闭的迷宫隧道；多趟迭代减少穿墙/抖动。
static void ResolveSolidCollision(MazeData *d) {
  Player *p = &d->cat;
  p->isOnTheGround = false;

  for (int pass = 0; pass < 2; pass++) {
    for (int i = 0; i < d->wallCount; i++) {
      Rectangle w = d->walls[i];
      Rectangle pr = PlayerRect(p);
      if (!CheckCollisionRecs(pr, w))
        continue;
      // 计算四个方向的穿透深度，沿最小穿透方向推出
      float penL = (pr.x + pr.width) - w.x;  // 玩家右缘越过墙左缘
      float penR = (w.x + w.width) - pr.x;   // 墙右缘越过玩家左缘
      float penT = (pr.y + pr.height) - w.y; // 玩家脚底越过墙顶
      float penB = (w.y + w.height) - pr.y;  // 墙底越过玩家头顶

      float min = penL;
      int dir = 0; // 0=左、1=右、2=顶(落地)、3=底(顶头)
      if (penR < min) {
        min = penR;
        dir = 1;
      }
      if (penT < min) {
        min = penT;
        dir = 2;
      }
      if (penB < min) {
        min = penB;
        dir = 3;
      }

      switch (dir) {
      case 0:
        p->position.x = w.x - pr.width;
        break;
      case 1:
        p->position.x = w.x + w.width;
        break;
      case 2:
        // 落在墙顶
        p->position.y = w.y - pr.height;
        p->velocity.y = 0.0f;
        p->isOnTheGround = true;
        break;
      default:
        // 顶头
        p->position.y = w.y + w.height;
        p->velocity.y = 0.0f;
        break;
      }
    }
  }
}

// 字母圆形落点是否与任意墙体相交（与拾取碰撞一致，用圆-矩形检测），
// 用于保证放下后不会卡进墙里。
static bool LetterDropCollides(const MazeData *d, Vector2 pos) {
  for (int i = 0; i < d->wallCount; i++) {
    if (CheckCollisionCircleRec(pos, LETTER_RADIUS, d->walls[i]))
      return true;
  }
  return false;
}

// 玩家放下字母时的落点：从玩家脚底向下找最近的可站立墙体顶面（兜底为地面）。
// 原实现用 `w.y <= feetY + 2` 会选中玩家脚底之上的天花板/侧墙，导致跳跃中
// 按 Z 放下时字母卡进天花板或墙里无法拾取；改为只取脚底之下（下落方向）
// 的墙体顶面，并加碰撞检测与水平试探兜底，保证落点开放可拾取。
static Vector2 DropLetterPosition(const MazeData *d, const Player *p) {
  const float feetX = p->position.x + p->size.x * 0.5f;
  const float feetY = p->position.y + p->size.y;
  float bestY = -1.0f;
  for (int i = 0; i < d->wallCount; i++) {
    Rectangle w = d->walls[i];
    if (feetX >= w.x && feetX <= w.x + w.width && w.y >= feetY - 2.0f) {
      if (bestY < 0.0f || w.y < bestY)
        bestY = w.y; // 取顶面位于脚底之下且最近的一道墙
    }
  }
  if (bestY < 0.0f)
    bestY = MAZE_GROUND_Y; // 兜底：地面顶面
  Vector2 pos = {feetX, bestY - 8.0f};
  // 落点若仍会卡进墙里（如脚底斜对墙体、四周被墙体包围），沿水平方向
  // 小步左右试探，取最近的开放落点；全部失败则退回原落点（兜底不悬空）。
  if (LetterDropCollides(d, pos)) {
    const float step = LETTER_RADIUS;
    for (int s = 1; s <= 6; s++) {
      Vector2 cand = {feetX + step * (float)s, pos.y};
      if (!LetterDropCollides(d, cand)) {
        pos = cand;
        break;
      }
      cand.x = feetX - step * (float)s;
      if (!LetterDropCollides(d, cand)) {
        pos = cand;
        break;
      }
    }
  }
  return pos;
}

// 字母放下落点解析（Character 组件回调）：复用 DropLetterPosition，
// 从玩家脚底向下找最近的可站立墙体顶面（兜底为地面），保证放下后可拾取。
static Vector2 MazeDropResolver(void *ctx, const Player *p) {
  MazeData *d = (MazeData *)ctx;
  return DropLetterPosition(d, p);
}

// 拼写正确：经过渡场景进入下一关（类型按 level_flow 权重刷新）。
// 通关第 MAX_LEVELS 关判定最终胜利：记录速通最佳时间并回到开始菜单。
static void MazeOnSpellCorrect(void *ctx) {
  MazeData *d = (MazeData *)ctx;
  // 通关奖励：恢复 5 点固定生命值（上限为最大生命值）
  PlayerHeal(&d->cat, CLEAR_HEALTH_REWARD);
  if (d->level >= MAX_LEVELS) {
    // 最终通关：记录速通最佳时间，经过渡进入通关结算场景
    // （scene_finish，最终胜利音效由该场景 onEnter 播放）
    SpeedrunFinish((GameApp *)d->app);
    GameStackReplace(d->owner,
                     TransitionSceneCreate(d->app, FinishSceneCreate(d->app)));
    return;
  }
  // 普通通关：播放通关单关音效（scene_battle 不计入），经过渡进入下一关
  GameAppPlaySound(d->app, d->app->levelFinishSound,
                   d->app->levelFinishSoundValid);
  GameStackReplace(d->owner, TransitionSceneCreate(
                                 d->app, LevelFlowCreateNextScene(
                                             d->app, d->level, d->difficulty)));
}

// 拼写错误：扣血并重置（字母放回原位由 Character 组件处理，需重新寻找）
static void MazeOnSpellWrong(void *ctx) {
  MazeData *d = (MazeData *)ctx;
  d->cat.health -= WRONG_PENALTY;
  if (d->cat.health < 0.0f)
    d->cat.health = 0.0f;
}

static void MazeUpdate(GameScene *self, float dt) {
  MazeData *d = (MazeData *)self->data;

  // HP 归零 → 失败
  if (d->cat.health <= 0.0f) {
    GameStackReplace(self->owner, FailSceneCreate(d->app));
    return;
  }

  // 倒计时：归零扣血并重置（给玩家继续本关的时间）
  d->timeLeft -= dt;
  if (d->timeLeft <= 0.0f) {
    d->cat.health -= TIME_PENALTY;
    d->timeLeft = MAZE_TIME_LIMIT;
  }

  // 平台跳跃物理：先更新（重力/跳跃/移动），再解析实心墙体碰撞
  UpdatePlayer(&d->cat, dt);
  ResolveSolidCollision(d);

  // 掉出世界底部 → 传送回出生房间（防御）
  if (d->cat.position.y > MAZE_WORLD_H) {
    d->cat.position = (Vector2){60.0f + 240.0f * (float)d->spawnRc,
                                MAZE_GROUND_Y - d->cat.size.y};
    d->cat.velocity = (Vector2){0, 0};
    d->cat.isOnTheGround = true;
  }

  CharacterUpdate(&d->character, &d->cat);

  // 相机跟随 + 动画帧
  SetCameraTarget(&d->camera, d->cat.position);
  UpdateSceneCamera(&d->camera, dt);
  d->source =
      AnimationUpdate(&d->cat.animations[d->cat.playerAnimationState], dt);
}

// HUD：顶部单词提示、左下角生命值条、右下角倒计时、右上角 ESC 提示、
// 底部居中的操作提示（Pick & Drop）。通用元素（生命值条/时间/ESC）
// 抽离到 tools/hud.h 供各场景复用。
static void DrawHud(MazeData *d) {
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;
  const float margin = 12.0f;
  const int fontSize = 16;

  // 左上角：当前关卡编号（与其他关卡 HUD 保持一致）
  HudDrawLevel(d->app, d->level);

  float topY = margin;
  const int hintSize = 28;
  char hint[128];
  snprintf(hint, sizeof(hint), "%s   (%s)", d->character.revealed,
           d->character.entry.pos);
  GameAppDrawText(d->app, hint,
                  (screenW - GameAppMeasureText(d->app, hint, hintSize)) / 2,
                  (int)topY, hintSize, BLACK);
  topY += (float)hintSize + 6.0f;

  // 中文释义（像素字体含中文字形，改为黑色便于阅读）
  const int meaningSize = 18;
  GameAppDrawText(
      d->app, d->character.entry.meaning,
      (screenW -
       GameAppMeasureText(d->app, d->character.entry.meaning, meaningSize)) /
          2,
      (int)topY, meaningSize, BLACK);

  // 左下角：生命值条（全局 HUD，传入继承后的当前 HP）
  HudDrawHealthBar(d->app, d->cat.health, d->cat.maxHealth);

  // 右下角：剩余倒计时（全局 HUD）
  HudDrawTime(d->app, d->timeLeft);

  // 右上角：ESC 暂停提示（全局 HUD）
  HudDrawEscHint(d->app);

  // 底部居中：操作提示（拾取/放下）。抬升到 HP 条上方，避免与左下角
  // HP 条/HP 文本、右下角计时器重合（HudDrawHealthBar 条高 16）。
  const char *help = "Pick & Drop : Z";
  const int helpSize = fontSize;
  const int helpY = screenH - (int)margin - 16 - helpSize - 6;
  GameAppDrawText(d->app, help,
                  (screenW - GameAppMeasureText(d->app, help, helpSize)) / 2,
                  helpY, helpSize, BLACK);
}

static void MazeDraw(GameScene *self) {
  MazeData *d = (MazeData *)self->data;

  BeginSceneCamera(&d->camera);

  // 迷宫背景：浅色底，挖空的隧道处露出背景
  DrawRectangle(-400, -400, (int)(MAZE_WORLD_W + 800),
                (int)(MAZE_WORLD_H + 800), Fade(DARKGRAY, 0.10f));

  // 纯色灰色墙体（无边框，简洁的迷宫切面观感）
  for (int i = 0; i < d->wallCount; i++) {
    DrawRectangleRec(d->walls[i], GRAY);
  }

  // 拼写平台：中央绿色高亮
  DrawRectangleRec(d->character.wordPlatform, Fade(GREEN, 0.75f));
  DrawRectangleLinesEx(d->character.wordPlatform, 1.0f, GREEN);

  // 拼写平台上方提示
  CharacterDrawSpellHint(&d->character, d->app);

  // 地上的字母（未被捡起的）
  CharacterDrawLetters(&d->character, d->app);

  // 玩家
  DrawPlayer(&d->cat, d->source);

  // 头顶字母 + 虚线引导回拼写平台
  CharacterDrawHeld(&d->character, d->app, &d->cat);

  EndSceneCamera(&d->camera);

  DrawHud(d);
}

static void MazeExit(GameScene *self) {
  MazeData *d = (MazeData *)self->data;
  // 保存当前 HP 供下一关继承（失败/回菜单时由开始场景重置为 0）
  ((GameApp *)d->app)->playerHealth = d->cat.health;
  UnloadTexture(d->cat.idleTexture);
  UnloadTexture(d->cat.runTexture);
  UnloadTexture(d->cat.jumpTexture);
  UnloadTexture(d->cat.sleepTexture);
  CharacterFreeBank(&d->character);
}

GameScene *MazeSceneCreate(const GameApp *app, int difficulty, int level) {
  GameScene *scene = (GameScene *)calloc(1, sizeof(GameScene));
  if (scene == NULL)
    return NULL;
  MazeData *data = (MazeData *)calloc(1, sizeof(MazeData));
  if (data == NULL) {
    free(scene);
    return NULL;
  }
  data->app = app;
  data->difficulty = difficulty;
  data->level = level;

  scene->name = "MazeScene";
  scene->data = data;
  scene->flags = GAME_SCENE_DRAW_WHEN_HIDDEN; // 暂停时仍绘制关卡作为背景
  scene->pauseable = true;                    // 关卡内允许按 ESC 调出暂停
  scene->onEnter = MazeEnter;
  scene->onUpdate = MazeUpdate;
  scene->onDraw = MazeDraw;
  scene->onExit = MazeExit;
  return scene;
}
