#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# 感谢Deepseek创建的项目一键式构建脚本
"""
build.py —— 一键构建并打包 Windows 与 Linux 安装程序，产物放入工作区目录。

产出格式
--------
  Windows/   Windows NSIS 安装向导（.exe）
  Linux/     DEB 安装包（.deb）+ RPM 安装包（.rpm）+ TGZ 压缩包（.tar.gz）

用法
----
    python build.py                   # 自动按当前平台构建
    python build.py --platform windows
    python build.py --platform linux   # 本机 Linux 或经 WSL
    python build.py --all              # 依次尝试 Windows 与 Linux

前提
----
  Windows: 已安装 CMake、MinGW（CMakePresets.json 中 gcc-mingw-release）、NSIS。
  Linux  (本机): cmake / gcc / python3 / dpkg-deb / rpmbuild（生成 rpm 需要）。
  Linux  (经 WSL): WSL 已安装，发行版内装好 cmake / gcc / python3 / dpkg-deb /
    rpmbuild，以及 raylib 依赖库：
      sudo apt install cmake build-essential python3 dpkg-dev rpm libgl1-mesa-dev
      libxi-dev libxcursor-dev libxrandr-dev libxinerama-dev libxkbcommon-dev
"""

import argparse
import glob
import os
import shutil
import subprocess
import sys

# 让中文输出在 UTF-8 终端（VSCode / 现代终端）正常显示，避免 GBK 乱码
if hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

# build.py 位于工作区根目录，ROOT 即脚本所在目录
ROOT = os.path.dirname(os.path.abspath(__file__))
OUT_DIRS = {"windows": "Windows", "linux": "Linux"}

PLATFORM_KEYS = {
    "win32": "windows",
    "cygwin": "windows",
    "darwin": "macos",
    "linux": "linux",
}


def detect_platform():
    """返回当前运行平台 key（windows / macos / linux）。"""
    return PLATFORM_KEYS.get(sys.platform, sys.platform)


def run(cmd, cwd=None, check=True):
    """执行命令；默认失败即抛错，返回 CompletedProcess。"""
    print(">>> " + " ".join(cmd))
    res = subprocess.run(cmd, cwd=cwd, check=False)
    if check and res.returncode != 0:
        raise RuntimeError("command failed (%d): %s" % (res.returncode,
                                                        " ".join(cmd)))
    return res


def run_wsl(bash_script):
    """在默认 WSL 发行版执行一段 bash 脚本。"""
    return run(["wsl", "-e", "bash", "-lc", bash_script], check=False)


def run_wsl_root(bash_script):
    """以 root 在默认 WSL 发行版执行 bash。
    打包 Linux 时 desktop 需写入系统目录 /usr/share/applications，故用 root。"""
    return run(["wsl", "-u", "root", "-e", "bash", "-lc", bash_script],
               check=False)


def wsl_available():
    try:
        res = subprocess.run(["wsl", "-l", "-q"], capture_output=True,
                             timeout=60)
        return res.returncode == 0
    except Exception:
        return False


def to_wsl_path(windows_path):
    """把 Windows 绝对路径转成 WSL 挂载路径（如 D:/... → /mnt/d/...）。"""
    drive, rest = os.path.splitdrive(os.path.abspath(windows_path))
    if not drive:
        return None
    return "/mnt/" + drive[0].lower() + rest.replace("\\", "/")


def find_makensis():
    """定位 makensis：先查 PATH，再查默认安装位置。"""
    p = shutil.which("makensis")
    if p:
        return p
    for cand in (r"C:\Program Files (x86)\NSIS\makensis.exe",
                 r"C:\Program Files\NSIS\makensis.exe"):
        if os.path.exists(cand):
            return cand
    return None


def copy_artifacts(pattern, dest_dir):
    """把匹配 pattern 的文件复制到 dest_dir，返回复制数量。"""
    os.makedirs(dest_dir, exist_ok=True)
    matches = glob.glob(pattern)
    for m in matches:
        if os.path.isfile(m):
            shutil.copy2(m, os.path.join(dest_dir, os.path.basename(m)))
    return len(matches)


def ensure_platform_dir(platform):
    # 兼容两种入参：小写键（"linux"）或目录名（"Linux"）
    d = OUT_DIRS.get(platform.lower(), platform)
    d = os.path.join(ROOT, d)
    os.makedirs(d, exist_ok=True)
    return d


# ── Windows ───────────────────────────────────────────────────────────────────
def build_windows():
    ensure_platform_dir("windows")
    print("\n===== [Windows] 构建 + NSIS 打包 =====")
    makensis = find_makensis()
    if not makensis:
        print("[Windows] 未找到 makensis（NSIS），请先安装 NSIS 或加入 PATH。")
        return False

    run(["cmake", "--preset", "gcc-mingw-release"], cwd=ROOT)
    run(["cmake", "--build", "--preset", "release"], cwd=ROOT)
    pkg = os.path.join(ROOT, "out", "package")
    cmd = ["cpack", "--preset", "nsis", "-B", pkg,
           "-D", "CPACK_NSIS_EXECUTABLE=" + makensis]
    run(cmd, cwd=ROOT)

    dest = ensure_platform_dir("windows")
    n = copy_artifacts(os.path.join(pkg, "CatET-*.exe"), dest)
    print("[Windows] 已放入 %s（%d 个文件）" % (dest, n))
    return n > 0


# ── Linux ─────────────────────────────────────────────────────────────────────
def pack_linux(build, dest):
    """用 CPack 依次生成 DEB / RPM / TGZ 并复制到 dest；返回成功产物数。"""
    pkgdir = os.path.join(build, "package")
    os.makedirs(pkgdir, exist_ok=True)
    cfg = os.path.join(build, "CPackConfig.cmake")
    for gen in ("DEB", "RPM", "TGZ"):
        try:
            run(["cpack", "--config", cfg, "-G", gen, "-B", pkgdir], cwd=ROOT)
        except RuntimeError as e:
            print("[Linux] %s 打包失败：%s" % (gen, e))
    n = copy_artifacts(os.path.join(pkgdir, "CatET-*.deb"), dest)
    n += copy_artifacts(os.path.join(pkgdir, "CatET-*.rpm"), dest)
    n += copy_artifacts(os.path.join(pkgdir, "CatET-*.tar.gz"), dest)
    return n


def build_linux_native():
    ensure_platform_dir("linux")
    print("\n===== [Linux] 原生构建 + 打包（DEB/RPM/TGZ）=====")
    for t in ("cmake", "gcc", "python3"):
        if not shutil.which(t):
            print("[Linux] 缺少工具 %s，请先安装。" % t)
            return False
    build = os.path.join(ROOT, "out", "build", "linux-release")
    run(["cmake", "-S", ROOT, "-B", build, "-DCMAKE_BUILD_TYPE=Release"],
        cwd=ROOT)
    run(["cmake", "--build", build], cwd=ROOT)
    dest = ensure_platform_dir("linux")
    n = pack_linux(build, dest)
    print("[Linux] 已放入 %s（%d 个文件）" % (dest, n))
    return n > 0


def build_linux_via_wsl():
    ensure_platform_dir("linux")
    print("\n===== [Linux] 通过 WSL 交叉构建 =====")
    if not wsl_available():
        print("[Linux] 未检测到可用的 WSL。请安装 WSL 并在发行版内装好工具链。")
        return False
    wsl_root = to_wsl_path(ROOT)
    if not wsl_root:
        print("[Linux] 无法把工作区路径映射为 WSL 路径。")
        return False

    # 1) 环境检查
    check = ("for t in cmake gcc python3 dpkg-deb git; do "
             "command -v $t >/dev/null 2>&1 || echo MISSING:$t; done")
    res = run_wsl_root("cd %s && %s" % (wsl_root, check))
    if "MISSING:" in (res.stdout or ""):
        print("[Linux] WSL 内缺少工具：\n%s\n请先在 WSL 安装："
              "\n  sudo apt install cmake build-essential python3 dpkg-dev rpm "
              "libgl1-mesa-dev libxi-dev libxcursor-dev libxrandr-dev "
              "libxinerama-dev libxkbcommon-dev" % res.stdout)
        return False

    # 国内网络访问 GitHub 不稳：优先复用已有 raylib 6.0 源码，否则从 Gitee
    # 镜像克隆到用户目录；构建时用 FETCHCONTENT_SOURCE_DIR_RAYLIB 指向本地。
    script = (
        "RL=${HOME}/raylib-6.0; "
        "if [ ! -d \"$RL\" ] && [ -d /home/xuantree/raylib-6.0 ]; then "
        "RL=/home/xuantree/raylib-6.0; fi; "
        "[ -d \"$RL\" ] || git clone --depth 1 --branch 6.0 "
        "https://gitee.com/mirrors/raylib.git \"$RL\"; "
        "cd %s && "
        "cmake -S . -B out/build/linux-release -DCMAKE_BUILD_TYPE=Release "
        "-DFETCHCONTENT_SOURCE_DIR_RAYLIB=\"$RL\" && "
        "cmake --build out/build/linux-release -j$(nproc) && "
        "mkdir -p out/build/linux-release/package Linux && "
        "cpack --config out/build/linux-release/CPackConfig.cmake -G DEB "
        "-B out/build/linux-release/package ; "
        "cpack --config out/build/linux-release/CPackConfig.cmake -G RPM "
        "-B out/build/linux-release/package ; "
        "cpack --config out/build/linux-release/CPackConfig.cmake -G TGZ "
        "-B out/build/linux-release/package ; "
        "cp -f out/build/linux-release/package/CatET-*.deb Linux/ ; "
        "cp -f out/build/linux-release/package/CatET-*.rpm Linux/ ; "
        "cp -f out/build/linux-release/package/CatET-*.tar.gz Linux/"
    ) % wsl_root
    res = run_wsl_root(script)
    if res.returncode != 0:
        print("[Linux] WSL 构建失败（返回码 %d）。常见原因：WSL 内缺 raylib "
              "依赖库或网络无法拉取 raylib。可手动执行以上命令定位。"
              % res.returncode)
        return False

    dest = ensure_platform_dir("linux")
    # 产物已由 WSL 脚本复制到工作区 Linux/，这里只统计校验
    # （避免把 Linux/ 里的文件复制回 Linux/ 自身导致 WinError 32）。
    n = len(glob.glob(os.path.join(ROOT, "Linux", "CatET-*.deb")))
    n += len(glob.glob(os.path.join(ROOT, "Linux", "CatET-*.rpm")))
    n += len(glob.glob(os.path.join(ROOT, "Linux", "CatET-*.tar.gz")))
    print("[Linux] 已放入 %s（%d 个文件）" % (dest, n))
    return n > 0


# ── 主流程 ────────────────────────────────────────────────────────────────────
def main():
    ap = argparse.ArgumentParser(
        description="一键构建并打包 Windows / Linux 安装程序")
    ap.add_argument("--platform", choices=["windows", "linux"],
                    help="只构建指定平台（不指定则按当前平台）")
    ap.add_argument("--all", action="store_true",
                    help="尝试构建 Windows 与 Linux 两个平台")
    args = ap.parse_args()

    current = detect_platform()
    print("当前平台: %s | 工作区: %s" % (current, ROOT))

    if args.platform:
        targets = [args.platform]
    elif args.all:
        targets = ["windows", "linux"]
    else:
        # 当前平台（macOS 不在支持范围，给出提示）
        targets = [current] if current in ("windows", "linux") else []
        if not targets:
            print("[提示] 当前平台 %s 不在构建范围内，请用 --platform "
                  "windows/linux 或 --all。" % current)

    results = {}
    for t in targets:
        try:
            if t == "windows":
                if current == "windows":
                    results[t] = build_windows()
                else:
                    print("[Windows] 非 Windows 平台，跳过。")
                    results[t] = False
            elif t == "linux":
                if current == "linux":
                    results[t] = build_linux_native()
                elif current == "windows":
                    results[t] = build_linux_via_wsl()
                else:
                    print("[Linux] 请在 Linux 或 Windows(WSL) 上构建。")
                    results[t] = False
        except Exception as e:  # 单个平台失败不中断其它平台
            print("[%s] 构建异常: %s" % (t, e))
            results[t] = False

    print("\n===== results =====")
    ok = True
    for t in targets:
        status = "OK" if results.get(t) else "FAILED / 跳过"
        if not results.get(t):
            ok = False
        print("  %-8s -> %s (%s)" % (t, status,
                                     os.path.join(ROOT, OUT_DIRS[t])))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
