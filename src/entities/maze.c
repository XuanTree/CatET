#include "entities/maze.h"
#include "tools/genrandom.h"
#include <stdbool.h>
#include <stdlib.h>

// ─────────────────────────────────────────────────────────────────────────────
// 迷宫生成实现：DFS 递归回溯生成完美迷宫（任意两格间恰有一条通路），
// 再在中心挖出 3x3 空地作为单词拼写区，最后统计死胡同供放置字母。
// ─────────────────────────────────────────────────────────────────────────────

// 每格记录四面墙是否存在；DFS 阶段还借用 visited 标记。
typedef struct MazeCell {
  bool north;
  bool south;
  bool west;
  bool east;
  bool visited;
} MazeCell;

#define MAZE_WALL_INIT_CAPACITY 256
#define MAZE_DEADEND_INIT_CAPACITY 64

void InitMaze(Maze *maze, int cols, int rows, float cellSize) {
  maze->cols = cols;
  maze->rows = rows;
  maze->cellSize = cellSize;
  maze->wallThickness = cellSize * 0.12f;
  maze->walls = NULL;
  maze->wallCount = 0;
  maze->wallCapacity = 0;
  maze->wordArea = (Rectangle){0};
  maze->startPos = (Vector2){0};
  maze->deadEnds = NULL;
  maze->deadEndCount = 0;
  maze->deadEndCapacity = 0;
}

// 就地打乱 4 个方向索引（0=北 1=南 2=西 3=东），保证每次生成的迷宫不同。
static void ShuffleDirs(int dirs[4]) {
  for (int i = 0; i < 4; i++)
    dirs[i] = i;
  for (int i = 3; i > 0; i--) {
    int j = genRandomNum(i + 1);
    int t = dirs[i];
    dirs[i] = dirs[j];
    dirs[j] = t;
  }
}

// DFS 递归回溯：从 (x,y) 出发随机打通未访问邻居之间的墙。
// 递归深度最大为 cols*rows（本作约 221），远低于默认栈容量，安全。
static void Carve(Maze *maze, MazeCell *cells, int x, int y) {
  int cols = maze->cols;
  cells[y * cols + x].visited = true;

  int dirs[4];
  ShuffleDirs(dirs);
  for (int d = 0; d < 4; d++) {
    int nx = x, ny = y;
    switch (dirs[d]) {
    case 0: ny = y - 1; break; // 北
    case 1: ny = y + 1; break; // 南
    case 2: nx = x - 1; break; // 西
    case 3: nx = x + 1; break; // 东
    }
    if (nx < 0 || nx >= maze->cols || ny < 0 || ny >= maze->rows)
      continue;
    if (cells[ny * cols + nx].visited)
      continue;

    // 打通当前格与邻居格之间的墙（两侧标志都要清除）。
    if (ny == y - 1) { // 北
      cells[y * cols + x].north = false;
      cells[ny * cols + nx].south = false;
    } else if (ny == y + 1) { // 南
      cells[y * cols + x].south = false;
      cells[ny * cols + nx].north = false;
    } else if (nx == x - 1) { // 西
      cells[y * cols + x].west = false;
      cells[ny * cols + nx].east = false;
    } else { // 东
      cells[y * cols + x].east = false;
      cells[ny * cols + nx].west = false;
    }
    Carve(maze, cells, nx, ny);
  }
}

// 追加一堵墙矩形；realloc 失败时静默丢弃该墙（降级，不崩溃）。
static void AddWall(Maze *maze, Rectangle r) {
  if (maze->wallCount >= maze->wallCapacity) {
    int newCap = (maze->wallCapacity == 0) ? MAZE_WALL_INIT_CAPACITY
                                           : maze->wallCapacity * 2;
    Rectangle *newWalls =
        (Rectangle *)realloc(maze->walls, sizeof(Rectangle) * (size_t)newCap);
    if (!newWalls)
      return;
    maze->walls = newWalls;
    maze->wallCapacity = newCap;
  }
  maze->walls[maze->wallCount++] = r;
}

// 追加一个死胡同格子中心。
static void AddDeadEnd(Maze *maze, Vector2 p) {
  if (maze->deadEndCount >= maze->deadEndCapacity) {
    int newCap = (maze->deadEndCapacity == 0) ? MAZE_DEADEND_INIT_CAPACITY
                                              : maze->deadEndCapacity * 2;
    Vector2 *newEnds =
        (Vector2 *)realloc(maze->deadEnds, sizeof(Vector2) * (size_t)newCap);
    if (!newEnds)
      return;
    maze->deadEnds = newEnds;
    maze->deadEndCapacity = newCap;
  }
  maze->deadEnds[maze->deadEndCount++] = p;
}

void GenerateMaze(Maze *maze) {
  if (maze->cols < 3 || maze->rows < 3)
    return;
  FreeMaze(maze); // 清理旧数据，支持重复生成

  int cols = maze->cols;
  int rows = maze->rows;
  int total = cols * rows;
  MazeCell *cells = (MazeCell *)calloc((size_t)total, sizeof(MazeCell));
  if (!cells)
    return;

  // 初始每格四面都有墙（闭合的格子），DFS 打通形成通路。
  for (int i = 0; i < total; i++) {
    cells[i].north = true;
    cells[i].south = true;
    cells[i].west = true;
    cells[i].east = true;
    cells[i].visited = false;
  }

  Carve(maze, cells, 0, 0);

  // 中心挖 3x3 空地：打通这 9 格之间的内部墙，形成一块可摆放单词的开放空间。
  int cx0 = cols / 2 - 1;
  int cy0 = rows / 2 - 1;
  for (int cy = cy0; cy < cy0 + 3; cy++) {
    for (int cx = cx0; cx < cx0 + 3; cx++) {
      if (cx < cx0 + 2) { // 打通东墙（与右侧格子连通）
        cells[cy * cols + cx].east = false;
        cells[cy * cols + cx + 1].west = false;
      }
      if (cy < cy0 + 2) { // 打通南墙（与下方格子连通）
        cells[cy * cols + cx].south = false;
        cells[(cy + 1) * cols + cx].north = false;
      }
    }
  }
  // 在空地上、下各开一个入口，让玩家能进出单词区。
  // 注意：分隔两格的墙统一由「下方格子的北墙」表示，因此下入口需清除
  // 空地底部格下方格子的 north，而非空地格自身的 south（后者只在最下行生成）。
  cells[cy0 * cols + (cx0 + 1)].north = false; // 上入口
  if (cy0 + 3 < rows)
    cells[(cy0 + 3) * cols + (cx0 + 1)].north = false; // 下入口

  // 把墙标志转换为墙壁矩形（每堵内部墙只用北/西墙唯一表示，避免重复）。
  float cs = maze->cellSize;
  float t = maze->wallThickness;
  for (int cy = 0; cy < rows; cy++) {
    for (int cx = 0; cx < cols; cx++) {
      MazeCell *c = &cells[cy * cols + cx];
      float x = cx * cs, y = cy * cs;
      if (c->north)
        AddWall(maze, (Rectangle){x, y, cs, t});
      if (c->west)
        AddWall(maze, (Rectangle){x, y, t, cs});
      if (cx == cols - 1 && c->east) // 最右列的东边界
        AddWall(maze, (Rectangle){x + cs - t, y, t, cs});
      if (cy == rows - 1 && c->south) // 最下行的南边界
        AddWall(maze, (Rectangle){x, y + cs - t, cs, t});
    }
  }

  // 统计死胡同（四面墙中仅一面打开），供放置字母。
  for (int cy = 0; cy < rows; cy++) {
    for (int cx = 0; cx < cols; cx++) {
      MazeCell *c = &cells[cy * cols + cx];
      int open = 0;
      if (!c->north)
        open++;
      if (!c->south)
        open++;
      if (!c->west)
        open++;
      if (!c->east)
        open++;
      if (open == 1) {
        AddDeadEnd(maze, (Vector2){cx * cs + cs * 0.5f, cy * cs + cs * 0.5f});
      }
    }
  }

  // 单词区 = 中心 3x3 空地的世界矩形；出生点 = 左上角格子中心。
  maze->wordArea = (Rectangle){cx0 * cs, cy0 * cs, 3 * cs, 3 * cs};
  maze->startPos = (Vector2){cs * 0.5f, cs * 0.5f};

  free(cells);
}

void DrawMaze(const Maze *maze) {
  if (!maze)
    return;
  for (int i = 0; i < maze->wallCount; i++) {
    DrawRectangleRec(maze->walls[i], DARKGRAY);
  }
  // 单词区以浅色铺底，提示玩家这是拼写区。
  DrawRectangleRec(maze->wordArea, Fade(LIGHTGRAY, 0.35f));
}

bool MazeCollide(const Maze *maze, Rectangle rect) {
  if (!maze)
    return false;
  for (int i = 0; i < maze->wallCount; i++) {
    if (CheckCollisionRecs(rect, maze->walls[i]))
      return true;
  }
  return false;
}

void FreeMaze(Maze *maze) {
  if (!maze)
    return;
  free(maze->walls);
  maze->walls = NULL;
  maze->wallCount = 0;
  maze->wallCapacity = 0;
  free(maze->deadEnds);
  maze->deadEnds = NULL;
  maze->deadEndCount = 0;
  maze->deadEndCapacity = 0;
}
