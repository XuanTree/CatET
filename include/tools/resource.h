/*
 * Copyright (C) 2026 XuanTree
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef RESOURCE_H
#define RESOURCE_H

#pragma once
#include <raylib.h>
#include <stddef.h>

// ─────────────────────────────────────────────────────────────────────────────
// 内嵌资源加载器（工具层）：
// 资源由 src/tools/pack_assets.py 编译成 C 字节数组并链接进可执行文件，
// 运行时不依赖外置 assets/ 目录（安装后不暴露资源文件）。
// path 一律使用 "assets/..." 形式的虚拟路径（正斜杠）。
// 加载失败返回空对象（Image.data==NULL / Texture.id==0 / Sound.frameCount==0 /
// Font.glyphCount==0），调用方自行判空降级，与原有文件加载的失败语义一致。
// ─────────────────────────────────────────────────────────────────────────────

// 取内嵌资源原始字节；未找到返回 NULL，*outSize 记录字节数。
const unsigned char *EmbeddedAssetGet(const char *path, size_t *outSize);

// 从内存解码为 Image（用完须 UnloadImage）。
Image LoadEmbeddedImage(const char *path);

// 从内存解码为 Texture2D（内部临时 Image 已自动释放）。
Texture2D LoadEmbeddedTexture(const char *path);

// 从内存解码为 Sound（内部临时 Wave 已自动释放）。
Sound LoadEmbeddedSound(const char *path);

// 从内存解码为 Font（等价 LoadFontEx 的码点/字号语义）。
Font LoadEmbeddedFontEx(const char *path, int fontSize, int *codepoints,
                        int cpCount);

#endif // RESOURCE_H
