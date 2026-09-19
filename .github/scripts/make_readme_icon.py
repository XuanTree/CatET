#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
make_readme_icon.py —— 为 README 生成放大版图标（不参与游戏打包）。

为什么要单独放一份
------------------
`assets/` 下的**任何**文件都会被 [`src/tools/pack_assets.py`](../../src/tools/pack_assets.py)
内嵌进可执行文件（`file(GLOB_RECURSE assets/*)`）。README 用的装饰图没必要占用
安装包体积，所以放大版图标写到 `.github/assets/` 下，与游戏资源彻底隔离。

做法
----
复用 [`src/tools/make_icons.py`](../../src/tools/make_icons.py) 里的 PNG 读取与
最近邻放大逻辑（纯标准库 `zlib` + `struct`，不依赖 PIL / ImageMagick），把
`assets/sprites/icon.png`（16x16 像素画）按**整数倍**放大后另存，
这样每个像素块边缘依然锐利，不会像浏览器平滑缩放那样发糊。

用法
----
    python .github/scripts/make_readme_icon.py            # 默认 256x256
    python .github/scripts/make_readme_icon.py 512        # 指定边长（须为 16 的整数倍）
    python .github/scripts/make_readme_icon.py 512 out.png

退出码: 0 成功；1 源图不可用；2 参数不合法。
"""

import os
import sys

# 导入 src/tools 下的模块时不生成 __pycache__，避免给仓库留下运行残留
sys.dont_write_bytecode = True

# .github/scripts/make_readme_icon.py → 仓库根目录
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "src", "tools"))

from make_icons import read_png, upscale_png  # noqa: E402（需先补 sys.path）

SRC_PNG = os.path.join(ROOT, "assets", "sprites", "icon.png")
DEFAULT_OUT = os.path.join(ROOT, ".github", "assets", "icon-256.png")
DEFAULT_SIZE = 256


def main():
    size = int(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_SIZE
    out = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUT

    if not os.path.isfile(SRC_PNG):
        print("error: 找不到源图标 %s" % SRC_PNG, file=sys.stderr)
        return 1

    # 保证不写进 assets/，否则会被 pack_assets.py 内嵌进可执行文件
    if os.path.abspath(out).startswith(os.path.join(ROOT, "assets") + os.sep):
        print("error: 输出不能放在 assets/ 下（会被打包进可执行文件）：%s" % out,
              file=sys.stderr)
        return 2

    try:
        png, w, h = read_png(SRC_PNG)
        if size <= 0 or size % w != 0 or size % h != 0:
            print("error: 目标边长 %d 必须是源图尺寸 %dx%d 的整数倍"
                  "（非整数倍会造成像素块大小不均）" % (size, w, h), file=sys.stderr)
            return 2
        data = upscale_png(png, w, h, size, size)
    except ValueError as exc:
        print("error: 无法处理 %s：%s（仅支持 8-bit RGB/RGBA 非隔行 PNG）"
              % (SRC_PNG, exc), file=sys.stderr)
        return 1

    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, "wb") as fp:
        fp.write(data)
    print("wrote %s（%dx%d，%d 字节，源图 %dx%d 放大 %d 倍）"
          % (os.path.relpath(out, ROOT).replace("\\", "/"), size, size, len(data),
             w, h, size // w))
    return 0


if __name__ == "__main__":
    sys.exit(main())
