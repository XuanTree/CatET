#ifndef ANIMATION_H
#define ANIMATION_H

#pragma once
#include <raylib.h>

typedef struct Animation {
  Texture2D *spritesheet; // 指向 spritesheet 纹理
  int frameCount;         // 总帧数
  int currentFrame;       // 当前帧索引
  float frameTime;        // 每帧持续时间（秒）
  float timer;            // 帧计时器
  int frameWidth;         // 每帧像素宽度
  int frameHeight;        // 每帧像素高度
  bool loop;              // 是否循环
} Animation;

// 初始化动画
inline void AnimationInit(Animation *anim, Texture2D *texture, int frameCount,
                          float frameTime, bool loop) {
  anim->spritesheet = texture;
  anim->frameCount = frameCount;
  anim->currentFrame = 0;
  anim->frameTime = frameTime;
  anim->timer = 0.0f;
  anim->frameWidth = texture->width / frameCount;
  anim->frameHeight = texture->height;
  anim->loop = loop;
}

// 每帧更新动画（返回当前帧的源矩形）
inline Rectangle AnimationUpdate(Animation *anim, float dt) {
  anim->timer += dt;
  if (anim->timer >= anim->frameTime) {
    anim->timer -= anim->frameTime;
    anim->currentFrame++;
    if (anim->currentFrame >= anim->frameCount) {
      if (anim->loop)
        anim->currentFrame = 0;
      else
        anim->currentFrame = anim->frameCount - 1;
    }
  }
  return (Rectangle){static_cast<float>(anim->currentFrame * anim->frameWidth),
                     0, static_cast<float>(anim->frameWidth),
                     static_cast<float>(anim->frameHeight)};
}

#endif // ANIMATION_H
