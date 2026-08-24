#include "scenes/scene_maze.h"
#include "entities/player.h"
#include "scenes/scene_fail.h"
#include "scenes/scene_transition.h"
#include "systems/level_flow.h"
#include "systems/words_loader.h"
#include "tools/camera.h"
#include "tools/genrandom.h"
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
// ─────────────────────────────────────────────────────────────────────────────

#define MAZE_TIME_LIMIT 180.0f // 迷宫关卡给更多时间（约 3 分钟，见游戏设计）
#define WRONG_PENALTY 20.0f    // 拼写错误扣血
#define TIME_PENALTY 20.0f     // 倒计时归零扣血（并重置倒计时）
#define LETTER_RADIUS 22.0f    // 字母拾取半径
#define MAZE_MAX_LETTERS 8
#define DISTRACTOR_COUNT 2 // 干扰字母数量（正确字母 1 + 干扰 2）

// ── 迷宫世界布局（网格化封闭迷宫，侧视如蚂蚁地穴）───────────────────────────
// 以 40px 网格砌出封闭的纵向迷宫：四周为实心边界墙，内部为
// 实心泥土挖出的蜿蜒隧道与竖井，中段设拼写平台。世界尺寸大于屏幕（1280×1440），
// 镜头跟随玩家，迷宫可超出屏幕外而不受影响。
#define MAZE_TILE 40.0f // 网格单元边长
#define MAZE_COLS 32    // 网格列数（32×40 = 1280 宽）
#define MAZE_ROWS 36    // 网格行数（36×40 = 1440 高）
#define MAZE_WORLD_W (MAZE_COLS * MAZE_TILE)
#define MAZE_WORLD_H (MAZE_ROWS * MAZE_TILE)
#define MAZE_GROUND_Y ((MAZE_ROWS - 1) * MAZE_TILE) // 地面顶面 y=1400
#define MAZE_MAX_LEVELS 10 // 迷宫画廊层数（房间网格 5 列 × 10 层）
#define MAZE_MAX_WALLS 768 // 墙体矩形数组上限

// 迷宫中的字母实体：正确字母或被挖掉字母的干扰项。
typedef struct MazeLetter {
  char ch;          // 字母字符
  bool isCorrect;   // 是否为需要的正确字母
  bool isPickedUp;  // 是否被玩家顶在头上
  Vector2 position; // 字母位置（世界坐标，落在某平台上方）
} MazeLetter;

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
  int spawnRc;            // 出生房间列（底部一层 rl=0）
  Rectangle wordPlatform; // 中央拼写平台（绿色高亮，顶面供玩家站立）
  Vector2 letterSpots[MAZE_MAX_LETTERS];      // 字母候选落点（迷宫各房间）
  bool letterSpotIsDeadEnd[MAZE_MAX_LETTERS]; // 对应落点所在房间是否为死胡同
  int letterSpotCount;
  WordsBank bank;    // 词库
  WordEntry entry;   // 当前单词
  char revealed[64]; // 挖空后的单词显示（'_' 表示空位）
  int blankIndex;    // 被挖空的字母在单词中的下标
  char answerChar;   // 正确字母
  MazeLetter letters[MAZE_MAX_LETTERS];
  int letterCount;
  float timeLeft;      // 关卡倒计时（秒）
  bool holdingLetter;  // 玩家是否顶着一个字母
  int heldLetterIndex; // 当前顶着的字母下标（-1 表示无）
  int difficulty;      // 难度（传给下一关）
  int level;           // 当前关卡编号（创建时注入，通关后经 level_flow 推进）
} MazeData;

// ── 迷宫布局（随机化 Prim 生成大型封闭式蚂蚁地穴迷宫）──────────────────────
// 以 40px 网格砌出封闭迷宫箱（四周实心边界墙，无需出口），内部用随机化
// Prim 算法在「房间网格」（5 列 × 10 层，每房间 6×2 格 = 240×80px）中
// 生长一个随机连通的分支隧道网络。世界尺寸大于屏幕，镜头跟随玩家。
//   - 房间水平相邻即连通；垂直间以「居中 2 格宽竖井」连通（楼板间隔 3 行
//     120px，可跳跃；竖井两侧各留 2 格实体地面，任何房间都有可站立之处，
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

// 房间（rc, rl）占 6×2 格：列 1+6*rc..6+6*rc，行 33-3*rl..34-3*rl；
// 楼板（下方）行 = 35-3*rl。
static void CarveRoom(MazeData *d, int rc, int rl) {
  CarveRect(d, 1 + rc * MAZE_ROOM_W, 33 - 3 * rl, MAZE_ROOM_W, 2);
}

// 房间 rl 与 rl+1 之间的竖井：居中 2 格宽，穿过楼板行 32-3*rl，
// 两侧各留 2 格实体供玩家站立（保证可达、不堵死）。
static void CarveShaft(MazeData *d, int rc, int lowerRl) {
  CarveRect(d, 3 + rc * MAZE_ROOM_W, 32 - 3 * lowerRl, 2, 1);
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
  return (Vector2){x, ((float)(35 - 3 * rl)) * MAZE_TILE - 8.0f};
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
  CarveRect(d, 1 + 2 * MAZE_ROOM_W, 33 - 3 * hubLevel, 2 * MAZE_ROOM_W, 2);
  carved[2][hubLevel] = true;
  carved[3][hubLevel] = true;
  int hubFloorRow = 35 - 3 * hubLevel; // 拼写大厅下方楼板行
  d->wordPlatform = (Rectangle){((float)(1 + 2 * MAZE_ROOM_W)) * MAZE_TILE,
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

// 抽取长度合适的单词并挖掉 1 个字母，再生成正确字母 + 干扰字母散落迷宫。
static void SetupWord(MazeData *d) {
  const WordEntry *entry = NULL;
  for (int i = 0; i < 200; i++) {
    const WordEntry *cand = WordsBankPickRandom(&d->bank);
    if (!cand)
      break;
    size_t len = strlen(cand->word);
    if (len >= 3 && len <= 12) {
      entry = cand;
      break;
    }
  }
  if (entry) {
    d->entry = *entry;
  } else {
    // 词库为空或没有合适长度：使用兜底单词，保证场景仍可运行
    snprintf(d->entry.word, sizeof(d->entry.word), "cat");
    snprintf(d->entry.meaning, sizeof(d->entry.meaning), "n. (fallback)");
    snprintf(d->entry.pos, sizeof(d->entry.pos), "n.");
  }

  size_t len = strlen(d->entry.word);
  d->blankIndex = genRandomNum((int)len);
  d->answerChar = d->entry.word[d->blankIndex];
  snprintf(d->revealed, sizeof(d->revealed), "%s", d->entry.word);
  d->revealed[d->blankIndex] = '_';

  // 干扰字母（不同于正确字母的小写字母）
  char distractors[DISTRACTOR_COUNT];
  for (int i = 0; i < DISTRACTOR_COUNT; i++) {
    char ch;
    do {
      ch = (char)('a' + genRandomNum(26));
    } while (ch == d->answerChar);
    distractors[i] = ch;
  }

  // 从程序化生成的迷宫平台动态收集候选落点（按层撒布），洗牌后随机分配，
  // 保证每局字母位置随布局变化，不再固定。洗牌时同步打乱死胡同标记，
  // 供下面把正确字母优先放进“非死胡同”房间。
  Vector2 spots[MAZE_MAX_LETTERS];
  bool spotIsDeadEnd[MAZE_MAX_LETTERS];
  int spotCount = CollectLetterSpots(d, spots, MAZE_MAX_LETTERS);
  for (int i = 0; i < spotCount; i++)
    spotIsDeadEnd[i] = d->letterSpotIsDeadEnd[i];
  for (int i = spotCount - 1; i > 0; i--) {
    int j = genRandomNum(i + 1);
    Vector2 t = spots[i];
    spots[i] = spots[j];
    spots[j] = t;
    bool tb = spotIsDeadEnd[i];
    spotIsDeadEnd[i] = spotIsDeadEnd[j];
    spotIsDeadEnd[j] = tb;
  }
  d->letterCount = 1 + DISTRACTOR_COUNT;
  if (d->letterCount > spotCount)
    d->letterCount = spotCount; // 落点不足时减少字母数（防御）
  // 正确字母优先放在“非死胡同”房间（deg>=2，可通过房间），保证
  // 出生点→正确字母→拼写平台之间无死路；全部为死胡同时退回随机（防御）。
  int correctIdx = -1;
  for (int i = 0; i < d->letterCount; i++) {
    if (!spotIsDeadEnd[i]) {
      correctIdx = i;
      break;
    }
  }
  if (correctIdx < 0)
    correctIdx = genRandomNum(d->letterCount);
  int distIdx = 0;
  for (int i = 0; i < d->letterCount; i++) {
    d->letters[i].isCorrect = (i == correctIdx);
    d->letters[i].ch =
        d->letters[i].isCorrect ? d->answerChar : distractors[distIdx++];
    d->letters[i].isPickedUp = false;
    d->letters[i].position = spots[i];
  }
  d->holdingLetter = false;
  d->heldLetterIndex = -1;
}

// ── 生命周期回调 ────────────────────────────────────────────────────────────

static void MazeEnter(GameScene *self) {
  MazeData *d = (MazeData *)self->data;

  d->cat = (Player){0};
  InitPlayer(&d->cat);
  // 生命值继承：进入新关卡时恢复上一关剩余 HP（新游戏 playerHealth=0 → 满血）
  if (d->app->playerHealth > 0.0f)
    d->cat.health = d->app->playerHealth;

  // 按难度加载词库
  const char *path;
  switch (d->difficulty) {
  case 2:
    path = "%sassets/words/CET6.txt";
    break;
  case 1:
    path = "%sassets/words/CET4.txt";
    break; // 普通暂用 CET4
  case 0:
  default:
    path = "%sassets/words/CET4.txt";
    break;
  }
  WordsBankLoad(&d->bank, TextFormat(path, GetApplicationDirectory()));

  // 构建封闭式蚂蚁地穴迷宫
  BuildMazeLayout(d);

  // 玩家出生在底部出生房间（地面之上）
  d->cat.position = (Vector2){60.0f + 240.0f * (float)d->spawnRc,
                              MAZE_GROUND_Y - d->cat.size.y};
  d->cat.velocity = (Vector2){0, 0};
  d->cat.isOnTheGround = true;

  // 跟随镜头
  InitSceneCamera(&d->camera, d->app->logicWidth, d->app->logicHeight, true,
                  CAMERA_FOLLOW_CENTER);

  // 抽词 + 挖空 + 放字母
  SetupWord(d);
  d->timeLeft = MAZE_TIME_LIMIT;
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

// 处理 Z 键：未持有字母时靠近字母则捡起；持有字母时在拼写平台上放下并判定，
// 在拼写平台以外任意位置放下仅归还字母（不判定、不扣血）。
static void HandleLetterInteraction(GameScene *self) {
  MazeData *d = (MazeData *)self->data;
  if (!IsKeyPressed(KEY_Z))
    return;

  Player *p = &d->cat;
  if (!d->holdingLetter) {
    for (int i = 0; i < d->letterCount; i++) {
      if (d->letters[i].isPickedUp)
        continue;
      if (CheckCollisionCircleRec(d->letters[i].position, LETTER_RADIUS,
                                  PlayerRect(p))) {
        d->letters[i].isPickedUp = true;
        d->holdingLetter = true;
        d->heldLetterIndex = i;
        break;
      }
    }
  } else {
    // 放下：玩家中心落在拼写平台及其上方空间内 → 拼写判定；
    // 否则可在任意位置放下字母，给玩家重新选择/放弃的余地，避免误扣血。
    Rectangle dropArea = {d->wordPlatform.x, d->wordPlatform.y - 48,
                          d->wordPlatform.width, d->wordPlatform.height + 48};
    Vector2 center = {p->position.x + p->size.x * 0.5f,
                      p->position.y + p->size.y * 0.5f};
    MazeLetter *held = &d->letters[d->heldLetterIndex];
    if (!CheckCollisionPointRec(center, dropArea)) {
      // 任意位置放下：字母落到玩家脚下的平台顶面（空中按 Z 也不会悬空）
      held->position = DropLetterPosition(d, p);
      held->isPickedUp = false;
      d->holdingLetter = false;
      d->heldLetterIndex = -1;
      return;
    }

    if (held->ch == d->answerChar) {
      // 拼写正确 → 经过渡场景进入下一关（类型按 level_flow 权重刷新）
      GameStackReplace(
          self->owner,
          TransitionSceneCreate(d->app, LevelFlowCreateNextScene(
                                            d->app, d->level, d->difficulty)));
    } else {
      // 拼写错误：扣血并重置（字母放回原位，需重新寻找）
      d->cat.health -= WRONG_PENALTY;
      if (d->cat.health < 0.0f)
        d->cat.health = 0.0f;
      held->isPickedUp = false;
      d->holdingLetter = false;
      d->heldLetterIndex = -1;
    }
  }
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

  HandleLetterInteraction(self);

  // 相机跟随 + 动画帧
  SetCameraTarget(&d->camera, d->cat.position);
  UpdateSceneCamera(&d->camera, dt);
  d->source =
      AnimationUpdate(&d->cat.animations[d->cat.playerAnimationState], dt);
}

// 绘制单个字母（统一颜色，不区分正确/错误，让玩家凭单词提示自行判断）。
static void DrawLetter(const GameApp *app, const MazeLetter *l) {
  DrawCircleV(l->position, LETTER_RADIUS, Fade(SKYBLUE, 0.85f));
  char txt[2] = {l->ch, '\0'};
  const int fs = 24;
  int w = GameAppMeasureText(app, txt, fs);
  GameAppDrawText(app, txt, (int)(l->position.x - w * 0.5f),
                  (int)(l->position.y - fs * 0.5f), fs, DARKBLUE);
}

// HUD：顶部单词提示、左下角 HP 条、右下角倒计时、右上角 ESC 提示、
// 底部居中的操作提示（Pick & Drop）。
static void DrawHud(MazeData *d) {
  const int screenW = d->app->logicWidth;
  const int screenH = d->app->logicHeight;
  const float margin = 12.0f;
  const int fontSize = 16;

  float topY = margin;
  const int hintSize = 28;
  char hint[128];
  snprintf(hint, sizeof(hint), "%s   (%s)", d->revealed, d->entry.pos);
  GameAppDrawText(d->app, hint,
                  (screenW - GameAppMeasureText(d->app, hint, hintSize)) / 2,
                  (int)topY, hintSize, BLACK);
  topY += (float)hintSize + 6.0f;

  // 中文释义（像素字体含中文字形，改为黑色便于阅读）
  const int meaningSize = 18;
  GameAppDrawText(
      d->app, d->entry.meaning,
      (screenW - GameAppMeasureText(d->app, d->entry.meaning, meaningSize)) / 2,
      (int)topY, meaningSize, BLACK);

  // 左下角：HP 条（颜色随剩余血量变化）
  const float barW = 150.0f, barH = 14.0f;
  const float barX = margin, barY = screenH - margin - barH;
  float ratio =
      (d->cat.maxHealth > 0.0f) ? d->cat.health / d->cat.maxHealth : 0.0f;
  if (ratio < 0.0f)
    ratio = 0.0f;
  if (ratio > 1.0f)
    ratio = 1.0f;
  DrawRectangle((int)barX, (int)barY, (int)barW, (int)barH, Fade(BLACK, 0.3f));
  Color hpColor = ratio > 0.5f ? GREEN : (ratio > 0.25f ? ORANGE : RED);
  DrawRectangle((int)barX, (int)barY, (int)(barW * ratio), (int)barH, hpColor);
  char hpText[32];
  snprintf(hpText, sizeof(hpText), "HP %d/%d", (int)d->cat.health,
           (int)d->cat.maxHealth);
  GameAppDrawText(d->app, hpText, (int)barX, (int)(barY - fontSize - 4),
                  fontSize, DARKGRAY);

  // 右下角：倒计时
  int sec = (int)d->timeLeft;
  char timeText[32];
  snprintf(timeText, sizeof(timeText), "Time %02d:%02d", sec / 60, sec % 60);
  int tw = GameAppMeasureText(d->app, timeText, fontSize);
  GameAppDrawText(d->app, timeText, screenW - (int)margin - tw,
                  screenH - (int)margin - fontSize, fontSize, DARKGRAY);

  // 右上角：ESC 提示
  const char *escText = "ESC";
  const int escW = GameAppMeasureText(d->app, escText, fontSize);
  const float boxW = (float)escW + 20.0f, boxH = (float)fontSize + 12.0f;
  const float boxX = (float)screenW - margin - boxW, boxY = margin;
  DrawRectangle((int)boxX, (int)boxY, (int)boxW, (int)boxH, Fade(BLACK, 0.55f));
  DrawRectangleLines((int)boxX, (int)boxY, (int)boxW, (int)boxH, DARKGRAY);
  GameAppDrawText(d->app, escText, (int)(boxX + (boxW - (float)escW) * 0.5f),
                  (int)(boxY + (boxH - (float)fontSize) * 0.5f), fontSize,
                  WHITE);

  // 底部居中：操作提示（拾取/放下）。恢复 16 字号，并抬升到 HP 条上方，
  // 避免与左下角 HP 条/HP 文本、右下角计时器重合。
  const char *help = "Pick & Drop : Z";
  const int helpSize = fontSize;
  const int helpY = screenH - (int)margin - (int)barH - helpSize - 6;
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
  DrawRectangleRec(d->wordPlatform, Fade(GREEN, 0.75f));
  DrawRectangleLinesEx(d->wordPlatform, 1.0f, GREEN);

  // 拼写平台上方提示（置于平台之上、上层楼板之下的走廊空腔中）
  const char *dropHint = "SPELL HERE";
  int dhSize = 16;
  int dhW = GameAppMeasureText(d->app, dropHint, dhSize);
  GameAppDrawText(
      d->app, dropHint,
      (int)(d->wordPlatform.x + d->wordPlatform.width * 0.5f - dhW * 0.5f),
      (int)(d->wordPlatform.y - dhSize - 8), dhSize, GREEN);

  // 地上的字母（未被捡起的）
  for (int i = 0; i < d->letterCount; i++) {
    if (!d->letters[i].isPickedUp)
      DrawLetter(d->app, &d->letters[i]);
  }

  // 玩家
  DrawPlayer(&d->cat, d->source);

  // 头顶字母 + 虚线引导回拼写平台
  if (d->holdingLetter && d->heldLetterIndex >= 0) {
    MazeLetter top = d->letters[d->heldLetterIndex];
    top.position = (Vector2){d->cat.position.x + d->cat.size.x * 0.5f,
                             d->cat.position.y - 12.0f};
    DrawLetter(d->app, &top);
    Vector2 to = {d->wordPlatform.x + d->wordPlatform.width * 0.5f,
                  d->wordPlatform.y};
    DrawLineEx(top.position, to, 2.0f, Fade(BLUE, 0.7f));
  }

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
  WordsBankFree(&d->bank);
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
  data->heldLetterIndex = -1;

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
