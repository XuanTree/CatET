#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# 感谢Deepseek提供的资源打包脚本
"""
pack_assets.py —— 把 assets/ 目录编译为 C 字节数组，嵌入游戏本体。

用法:
    python pack_assets.py <assets_dir> <output_dir>

生成:
    <output_dir>/embedded_assets.h  资源表结构 + 查找接口声明
    <output_dir>/embedded_assets.c  全部资源的字节数组 + 查找表实现

设计说明:
    - 游戏运行时不再依赖外置 assets/ 目录（安装后不暴露资源文件），
      所有贴图/音效/词库/字体都从内存加载（见 src/tools/resource.c）。
    - 仅排除 .gitkeep 与隐藏文件（. 开头）；其余文件原样嵌入。
    - 路径统一用正斜杠，与代码中 "assets/..." 引用一致。
"""

import os
import sys


def collect_assets(root):
    """递归收集 assets 文件，返回按路径排序的相对路径列表（正斜杠）。"""
    found = []
    for dirpath, dirnames, filenames in os.walk(root):
        # 跳过隐藏目录
        dirnames[:] = [d for d in dirnames if not d.startswith(".")]
        for name in filenames:
            if name.startswith(".") or name == ".gitkeep":
                continue
            full = os.path.join(dirpath, name)
            # 统一加 "assets/" 前缀，与代码中 "assets/..." 引用保持一致
            rel = "assets/" + os.path.relpath(full, root).replace("\\", "/")
            found.append(rel)
    found.sort()
    return found


def c_array_literal(data, indent="    "):
    """把 bytes 转成 C 字节数组字面量。"""
    lines = []
    line = []
    for i, b in enumerate(data):
        line.append("0x%02x," % b)
        if len(line) == 12:
            lines.append(indent + " ".join(line))
            line = []
    if line:
        lines.append(indent + " ".join(line))
    return "\n".join(lines)


def write_header(out_h, rels):
    header = """/*
 * embedded_assets.h —— 自动生成，请勿手改！
 * 由 src/tools/pack_assets.py 生成，包含全部内嵌资源的查找接口。
 */
#ifndef EMBEDDED_ASSETS_H
#define EMBEDDED_ASSETS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// 内嵌资源条目：path 为 "assets/..." 形式（正斜杠）
typedef struct EmbeddedAsset {
    const char *path;
    const unsigned char *data;
    unsigned int size;
} EmbeddedAsset;

// 按 path 精确查找资源；未找到返回 NULL
const EmbeddedAsset *EmbeddedAssetFind(const char *path);

// 便捷取数据接口：返回资源字节指针，*outSize 记录大小；未找到返回 NULL
const unsigned char *EmbeddedAssetGet(const char *path, size_t *outSize);

// 内嵌资源总数
int EmbeddedAssetCount(void);

// 已嵌入的资源路径清单（供调试/断言使用）
#define EMBEDDED_ASSET_PATHS \\
%(paths_macro)s

#ifdef __cplusplus
}
#endif

#endif /* EMBEDDED_ASSETS_H */
""" % {"paths_macro": " \\\n".join('    "%s"' % r for r in rels)}
    with open(out_h, "w", encoding="utf-8") as f:
        f.write(header)


def write_source(out_c, root, rels):
    parts = []
    parts.append("""/*
 * embedded_assets.c —— 自动生成，请勿手改！
 * 由 src/tools/pack_assets.py 生成，包含全部内嵌资源的字节数据。
 */
#include <string.h>
#include "embedded_assets.h"

""")
    for i, rel in enumerate(rels):
        # rel 形如 "assets/.../xx.png"，去掉 "assets/" 前缀后相对 assets 根目录读取
        inner = rel.split("/", 1)[1] if "/" in rel else rel
        full = os.path.join(root, inner.replace("/", os.sep))
        with open(full, "rb") as fp:
            data = fp.read()
        parts.append("static const unsigned char g_%d[] = {\n%s\n};\n\n" %
                     (i, c_array_literal(data)))
    parts.append("static const EmbeddedAsset kAssets[] = {\n")
    for i, rel in enumerate(rels):
        parts.append('    {"%s", g_%d, sizeof(g_%d)},\n' % (rel, i, i))
    parts.append("""};

const EmbeddedAsset *EmbeddedAssetFind(const char *path) {
    if (!path)
        return NULL;
    for (int i = 0; i < (int)(sizeof(kAssets) / sizeof(kAssets[0])); i++) {
        if (strcmp(kAssets[i].path, path) == 0)
            return &kAssets[i];
    }
    return NULL;
}

const unsigned char *EmbeddedAssetGet(const char *path, size_t *outSize) {
    if (outSize)
        *outSize = 0;
    const EmbeddedAsset *a = EmbeddedAssetFind(path);
    if (!a)
        return NULL;
    if (outSize)
        *outSize = a->size;
    return a->data;
}

int EmbeddedAssetCount(void) {
    return (int)(sizeof(kAssets) / sizeof(kAssets[0]));
}
""")
    with open(out_c, "w", encoding="utf-8") as f:
        f.write("".join(parts))


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(2)
    assets_dir = os.path.abspath(sys.argv[1])
    out_dir = os.path.abspath(sys.argv[2])
    if not os.path.isdir(assets_dir):
        print("error: assets dir not found: %s" % assets_dir, file=sys.stderr)
        sys.exit(1)
    os.makedirs(out_dir, exist_ok=True)

    rels = collect_assets(assets_dir)
    if not rels:
        print("error: no assets found under %s" % assets_dir, file=sys.stderr)
        sys.exit(1)

    out_h = os.path.join(out_dir, "embedded_assets.h")
    out_c = os.path.join(out_dir, "embedded_assets.c")
    write_header(out_h, rels)
    write_source(out_c, assets_dir, rels)
    print("packed %d assets -> %s" % (len(rels), out_dir))


if __name__ == "__main__":
    main()
