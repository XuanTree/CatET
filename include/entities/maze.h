#ifndef MAZE_H
#define MAZE_H

#pragma once
#include <raylib.h>

// ─────────────────────────────────────────────────────────────────────────────
// 程序化生成的 2D 迷宫（由 Rectangle 墙壁组成），供迷宫解密关卡使用。
// 采用 DFS 递归回溯生成完美迷宫，并在中心挖出一块 3x3 空地用于摆放单词。
// 迷宫无需 assets/maps/ 预置地图，全部由代码生成（见 repo_structure.md）。
// ─────────────────────────────────────────────────────────────────────────────

typedef struct Maze {
  int cols;             // 单元格列数
  int rows;             // 单元格行数
  float cellSize;       // 单元格像素尺寸（玩家一格宽）
  float wallThickness;  // 墙壁厚度（像素）
  Rectangle *walls;     // 墙壁矩形数组（含边界墙）
  int wallCount;
  int wallCapacity;
  Rectangle wordArea;   // 单词拼写空地（世界坐标矩形，中心 3x3 区域）
  Vector2 startPos;     // 玩家出生点（左上角格子中心，世界坐标）
  Vector2 *deadEnds;    // 死胡同格子中心坐标（供放置字母）
  int deadEndCount;
  int deadEndCapacity;
} Maze;

// 初始化迷宫参数（尚未生成），设置网格尺寸与单元格大小。
void InitMaze(Maze *maze, int cols, int rows, float cellSize);

// 生成迷宫：DFS 打通通路 + 中心挖 3x3 空地 + 统计死胡同 + 计算出生点/单词区。
// 需先 InitMaze；结果写入 walls / wordArea / startPos / deadEnds。
void GenerateMaze(Maze *maze);

// 绘制迷宫墙壁与单词区空地（世界坐标，需在场景相机内调用）。
void DrawMaze(const Maze *maze);

// 检测矩形是否与任意墙壁重叠（用于玩家碰撞）。
bool MazeCollide(const Maze *maze, Rectangle rect);

// 释放墙壁与死胡同数组。
void FreeMaze(Maze *maze);

#endif // MAZE_H
