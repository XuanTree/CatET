#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
fetch_font.py —— 为构建补回内嵌 UI 字体（fusion-pixel-font）。

背景
----
仓库不收录 assets/fonts/pixel_font.ttf（许可原因，见 README 的
"Why is the font missing from the repo?"），但发布包必须带上它，否则游戏里的
中文界面只能退化成 raylib 内置字体。CI 在构建前调用本脚本，把与本地开发一致的
「12px 等宽 · 简体中文」字体取回并放到 assets/fonts/pixel_font.ttf；
取回后由 src/tools/pack_assets.py 一并内嵌进可执行文件。

用法:
    python .github/scripts/fetch_font.py [目标路径]

环境变量（可选，用于换源或升级字体版本）:
    CATET_FONT_URL       直接指向 .ttf 的地址（优先，设置后不再走 zip 方式）
    CATET_FONT_ZIP       融合像素字体发布包 zip 的地址
    CATET_FONT_ENTRY     zip 内要提取的 ttf 文件名
    CATET_FONT_MIN_SIZE  判定为有效字体的最小字节数（默认 1 MiB）

退出码: 0 成功（或目标已存在）；1 获取失败（CI 里按「降级构建」处理）。
"""

import io
import os
import sys
import urllib.request
import zipfile

# Windows 控制台默认编码是 cp1252 / GBK，直接 print 中文会抛
# UnicodeEncodeError，把「只是打日志」的步骤变成红色失败（CI 上踩过）。
# 因此在任何 print 之前强制 UTF-8，并让不可编码字符降级替换而不是抛异常。
if sys.platform == "win32":
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError):
            pass

# 与本地开发使用的同一版本：fusion-pixel-font 12px monospaced zh_hans
FONT_VERSION = "2026.09.01"
FONT_ZIP = ("https://github.com/TakWolf/fusion-pixel-font/releases/download/"
            "%s/fusion-pixel-font-12px-monospaced-ttf-v%s.zip"
            % (FONT_VERSION, FONT_VERSION))
FONT_ENTRY = "fusion-pixel-12px-monospaced-zh_hans.ttf"
DEFAULT_TARGET = os.path.join("assets", "fonts", "pixel_font.ttf")
DEFAULT_MIN_SIZE = 1024 * 1024


def log(msg):
    """统一带前缀输出，方便在 CI 日志里定位。"""
    print("[font] %s" % msg)


def download(url, timeout=300):
    """下载 URL 并返回 bytes。"""
    log("下载 %s" % url)
    req = urllib.request.Request(url, headers={"User-Agent": "CatET-CI"})
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return resp.read()


def extract_ttf(blob, entry):
    """从 zip 中取出目标 ttf；优先精确文件名，其次按语言/后缀兜底。"""
    with zipfile.ZipFile(io.BytesIO(blob)) as zf:
        names = zf.namelist()
        if entry and entry in names:
            log("提取 %s" % entry)
            return zf.read(entry)
        for keyword in ("zh_hans", "zh_hant", ""):
            for name in names:
                if name.endswith(".ttf") and keyword in name:
                    log("提取 %s（兜底匹配 %r）" % (name, keyword or ".ttf"))
                    return zf.read(name)
    raise RuntimeError("发布包内没有可用的 .ttf：%s" % ", ".join(names))


def main():
    target = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_TARGET
    min_size = int(os.environ.get("CATET_FONT_MIN_SIZE") or DEFAULT_MIN_SIZE)

    if os.path.isfile(target) and os.path.getsize(target) >= min_size:
        log("%s 已存在（%d 字节），跳过下载"
            % (target, os.path.getsize(target)))
        return 0

    os.makedirs(os.path.dirname(os.path.abspath(target)), exist_ok=True)

    try:
        direct_url = os.environ.get("CATET_FONT_URL")
        if direct_url:
            data = download(direct_url)
        else:
            zip_url = os.environ.get("CATET_FONT_ZIP") or FONT_ZIP
            entry = os.environ.get("CATET_FONT_ENTRY") or FONT_ENTRY
            data = extract_ttf(download(zip_url), entry)
    except Exception as exc:  # 网络/上游异常不应直接掐断构建
        log("获取字体失败：%s" % exc)
        log("本次构建将使用 raylib 内置字体（中文界面显示会退化）")
        return 1

    if len(data) < min_size:
        log("字体数据异常（%d 字节 < 下限 %d），未写入" % (len(data), min_size))
        return 1

    with open(target, "wb") as fp:
        fp.write(data)
    log("已写入 %s（%d 字节）" % (target, len(data)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
