#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
make_icons.py —— 从 assets/sprites/icon.png 生成打包/安装用的图标：

    packaging/CatET.ico   Windows 图标（ICO，内嵌 PNG 数据，Vista+ 兼容）
    packaging/CET.icns    macOS 图标（ICNS，内嵌 PNG 数据）

用法:
    python make_icons.py <icon.png> <output_dir>

说明:
    - ICO / ICNS 容器都允许直接内嵌 PNG 压缩数据（无需解压像素），
      因此本脚本只做容器封装，不依赖 PIL / ImageMagick 等第三方库。
    - ICO 的 PNG 条目需要 Windows Vista 及以上系统才能识别（本项目的
      NSIS 安装向导与 MinGW 生成的 exe 均满足）。
    - ICNS 使用 icp4（16x16 PNG）与 ic08（256x256 PNG）两种类型，
      256 由 16 原图最近邻放大得到（pixel-art 风格，放大后仍清晰）。
"""

import os
import struct
import sys

# ICNS 支持的 PNG 类型标识与尺寸
ICNS_TYPES = [
    (b"icp4", 16),  # 16x16 PNG
    (b"ic08", 256),  # 256x256 PNG
]


def read_png(path):
    """读取 PNG 文件，返回 (raw_bytes, width, height)。"""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG file: %s" % path)
    # IHDR: 8(签名) + 4(长度) + 4("IHDR") + 13(数据) ...
    w, h = struct.unpack(">II", data[16:24])
    return data, w, h


def upscale_png(data, src_w, src_h, dst_w, dst_h):
    """最近邻放大 PNG（仅处理 8-bit RGBA/RGB，非隔行扫描）。"""
    # 解析 IHDR
    bit_depth = data[24]
    color_type = data[25]
    interlace = data[28]
    if interlace != 0:
        raise ValueError("interlaced PNG not supported")
    bpp = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}.get(color_type)
    if bpp is None:
        raise ValueError("unsupported color type %d" % color_type)

    # 拼接所有 IDAT 数据
    pos = 8
    idat = b""
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        ctype = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        if ctype == b"IDAT":
            idat += chunk
        pos += 12 + length

    import zlib
    raw = zlib.decompress(idat)

    # 行过滤（Paeth / Sub / Up / Avg / None）
    stride = src_w * bpp
    rows = []
    pos = 0
    prev = bytearray(stride)
    for _ in range(src_h):
        ftype = raw[pos]
        pos += 1
        line = bytearray(raw[pos:pos + stride])
        pos += stride
        if ftype == 1:  # Sub
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 0xFF
        elif ftype == 2:  # Up
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif ftype == 3:  # Average
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif ftype == 4:  # Paeth
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        rows.append(bytes(line))
        prev = line

    # 组装 RGBA 像素
    pixels = bytearray()
    for row in rows:
        if color_type == 6:  # RGBA
            pixels += row
        elif color_type == 2:  # RGB -> RGBA
            for i in range(0, stride, 3):
                pixels += row[i:i + 3] + b"\xff"
        else:
            raise ValueError("unsupported color type %d for upscale" % color_type)

    # 最近邻放大
    out = bytearray()
    for y in range(dst_h):
        sy = int(y * src_h / dst_h)
        base = sy * src_w * 4
        for x in range(dst_w):
            sx = int(x * src_w / dst_w)
            p = (base + sx * 4)
            out += pixels[p:p + 4]
    return encode_png(bytes(out), dst_w, dst_h)


def encode_png(rgba, w, h):
    """把 RGBA 像素编码为 PNG（8-bit，非隔行）。"""
    import zlib
    raw = bytearray()
    for y in range(h):
        raw.append(0)  # filter: None
        row = rgba[y * w * 4:(y + 1) * w * 4]
        raw += row

    def chunk(ctype, cdata):
        return (struct.pack(">I", len(cdata)) + ctype + cdata +
                struct.pack(">I", zlib.crc32(ctype + cdata) & 0xFFFFFFFF))

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", ihdr) +
            chunk(b"IDAT", zlib.compress(bytes(raw), 9)) +
            chunk(b"IEND", b""))


def build_ico(png_bytes, w, h):
    """把 PNG 封装进 ICO 容器（单条目 PNG 类型）。"""
    count = 1
    header = struct.pack("<HHH", 0, 1, count)
    entry = struct.pack("<BBBBHHII",
                        w if w < 256 else 0,
                        h if h < 256 else 0,
                        0, 0, 1, 32, len(png_bytes), 6 + 16)
    return header + entry + png_bytes


def build_icns(png_by_size):
    """把 {type: png_bytes} 封装进 ICNS 容器。"""
    body = b""
    for ctype, png in png_by_size:
        body += ctype + struct.pack(">I", len(png) + 8) + png
    total = 8 + len(body)
    return b"icns" + struct.pack(">I", total) + body


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(2)
    png_path = os.path.abspath(sys.argv[1])
    out_dir = os.path.abspath(sys.argv[2])
    if not os.path.isfile(png_path):
        print("error: icon not found: %s" % png_path, file=sys.stderr)
        sys.exit(1)
    os.makedirs(out_dir, exist_ok=True)

    png, w, h = read_png(png_path)
    print("source icon: %dx%d %s" % (w, h, png_path))

    # ICO：直接用原 PNG（16x16）
    ico_path = os.path.join(out_dir, "CatET.ico")
    with open(ico_path, "wb") as f:
        f.write(build_ico(png, w, h))
    print("wrote %s" % ico_path)

    # ICNS：16 原图 + 256 放大
    png_256 = upscale_png(png, w, h, 256, 256)
    icns_path = os.path.join(out_dir, "CET.icns")
    with open(icns_path, "wb") as f:
        f.write(build_icns([(b"icp4", png), (b"ic08", png_256)]))
    print("wrote %s" % icns_path)


if __name__ == "__main__":
    main()
