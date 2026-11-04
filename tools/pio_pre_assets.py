"""
PlatformIO Pre-Build Hook (tools/pio_pre_assets.py)
---------------------------------------------------
Automatically generates `build/logo.bin` (+ `include/logo_offsets.h`) and
`build/anim.bin` (+ `include/anim_offsets.h`) before compilation whenever
assets in `assets/logo/` or `assets/anim/` are added or modified.
"""

import os
import subprocess
import sys
from os.path import join

Import("env")

project_dir = env.subst("$PROJECT_DIR")
mk_logo_script = join(project_dir, "tools", "mk_logo_bin.py")
mk_anim_script = join(project_dir, "tools", "mk_anim_bin.py")
mk_icon_script = join(project_dir, "tools", "mk_icon_c.py")

logo_bin_path = join(project_dir, "build", "logo.bin")
logo_hdr_path = join(project_dir, "include", "bootloader", "logo_offsets.h")
logo_assets_dir = join(project_dir, "assets", "logo")

anim_bin_path = join(project_dir, "build", "anim.bin")
anim_hdr_path = join(project_dir, "include", "bootloader", "anim_offsets.h")
anim_assets_dir = join(project_dir, "assets", "anim")

icons_out_dir = join(project_dir, "src", "os", "ui", "icons")
icon_svg_path = join(project_dir, "assets", "icons", "ic_os_white.svg")
icon_c_path = join(icons_out_dir, "img_ic_os_white.c")


def dir_newer_than(folder: str, target_mtime: float) -> bool:
    if not os.path.isdir(folder):
        return False
    for fname in os.listdir(folder):
        fpath = join(folder, fname)
        if os.path.isfile(fpath) and os.path.getmtime(fpath) > target_mtime:
            return True
    return False


# 1. Check / build logo.bin
if not os.path.isfile(logo_bin_path) or not os.path.isfile(logo_hdr_path):
    subprocess.check_call([sys.executable, mk_logo_script])
else:
    mtime = min(os.path.getmtime(logo_bin_path), os.path.getmtime(logo_hdr_path))
    if dir_newer_than(logo_assets_dir, mtime) or os.path.getmtime(mk_logo_script) > mtime:
        subprocess.check_call([sys.executable, mk_logo_script])

# 2. Check / build anim.bin
if not os.path.isfile(anim_bin_path) or not os.path.isfile(anim_hdr_path):
    subprocess.check_call([sys.executable, mk_anim_script])
else:
    mtime = min(os.path.getmtime(anim_bin_path), os.path.getmtime(anim_hdr_path))
    if dir_newer_than(anim_assets_dir, mtime) or os.path.getmtime(mk_anim_script) > mtime:
        subprocess.check_call([sys.executable, mk_anim_script])

# 3. Check / build UI icons in src/os/ui/icons/
icon_5j_path = join(project_dir, "assets", "icons", "5J.png")
icon_5j_c_path = join(icons_out_dir, "img_ic_airline_5j.c")
icon_pr_path = join(project_dir, "assets", "icons", "PR.png")
icon_pr_c_path = join(icons_out_dir, "img_ic_airline_pr.c")
icon_tg_path = join(project_dir, "assets", "icons", "TG.png")
icon_tg_c_path = join(icons_out_dir, "img_ic_airline_tg.c")
icon_a320_path = join(project_dir, "assets", "icons", "a320.svg")
icon_a320_c_path = join(icons_out_dir, "img_ac_a320.c")
icon_a330_path = join(project_dir, "assets", "icons", "a330.svg")
icon_a330_c_path = join(icons_out_dir, "img_ac_a330.c")
icon_sep_path = join(project_dir, "assets", "icons", "ic_flight_separator.svg")
icon_sep_c_path = join(icons_out_dir, "img_ic_flight_separator.c")
icon_wp_path = join(project_dir, "assets", "icons", "wallpaper.png")
icon_wp_c_path = join(icons_out_dir, "img_wallpaper.c")
icon_conn_path = join(project_dir, "assets", "icons", "ic_connect.svg")
icon_conn_c_path = join(icons_out_dir, "img_ic_connect.c")

rebuild_icons = False
if os.path.isfile(icon_svg_path) and (not os.path.isfile(icon_c_path) or os.path.getmtime(icon_svg_path) > os.path.getmtime(icon_c_path)):
    rebuild_icons = True
if os.path.isfile(icon_5j_path) and (not os.path.isfile(icon_5j_c_path) or os.path.getmtime(icon_5j_path) > os.path.getmtime(icon_5j_c_path)):
    rebuild_icons = True
if os.path.isfile(icon_pr_path) and (not os.path.isfile(icon_pr_c_path) or os.path.getmtime(icon_pr_path) > os.path.getmtime(icon_pr_c_path)):
    rebuild_icons = True
if os.path.isfile(icon_tg_path) and (not os.path.isfile(icon_tg_c_path) or os.path.getmtime(icon_tg_path) > os.path.getmtime(icon_tg_c_path)):
    rebuild_icons = True
if os.path.isfile(icon_a320_path) and (not os.path.isfile(icon_a320_c_path) or os.path.getmtime(icon_a320_path) > os.path.getmtime(icon_a320_c_path)):
    rebuild_icons = True
if os.path.isfile(icon_a330_path) and (not os.path.isfile(icon_a330_c_path) or os.path.getmtime(icon_a330_path) > os.path.getmtime(icon_a330_c_path)):
    rebuild_icons = True
if os.path.isfile(icon_sep_path) and (not os.path.isfile(icon_sep_c_path) or os.path.getmtime(icon_sep_path) > os.path.getmtime(icon_sep_c_path)):
    rebuild_icons = True
if os.path.isfile(icon_wp_path) and (not os.path.isfile(icon_wp_c_path) or os.path.getmtime(icon_wp_path) > os.path.getmtime(icon_wp_c_path)):
    rebuild_icons = True
if os.path.isfile(icon_conn_path) and (not os.path.isfile(icon_conn_c_path) or os.path.getmtime(icon_conn_path) > os.path.getmtime(icon_conn_c_path)):
    rebuild_icons = True
if os.path.getmtime(mk_icon_script) > os.path.getmtime(icon_c_path) if os.path.isfile(icon_c_path) else True:
    rebuild_icons = True

if rebuild_icons:
    subprocess.check_call([sys.executable, mk_icon_script])

# 4. Ensure nested build directories are pre-created to avoid SCons parallel build race condition
build_dir = env.subst("$BUILD_DIR")
os.makedirs(build_dir, exist_ok=True)
os.makedirs(join(build_dir, "FrameworkCMSIS", "GCC"), exist_ok=True)
os.makedirs(join(build_dir, "FrameworkSPL"), exist_ok=True)
src_dir = join(project_dir, "src")
if os.path.isdir(src_dir):
    for root, dirs, _ in os.walk(src_dir):
        for d in dirs:
            rel = os.path.relpath(join(root, d), src_dir)
            os.makedirs(join(build_dir, "src", rel), exist_ok=True)

libdeps_dir = join(project_dir, ".pio", "libdeps", env.subst("$PIOENV"))
if os.path.isdir(libdeps_dir):
    for libname in os.listdir(libdeps_dir):
        lib_src = join(libdeps_dir, libname, "src")
        if os.path.isdir(lib_src):
            for root, dirs, _ in os.walk(lib_src):
                for d in dirs:
                    rel = os.path.relpath(join(root, d), lib_src)
                    os.makedirs(join(build_dir, "lib071", libname, rel), exist_ok=True)
                    for entry in os.listdir(build_dir):
                        if entry.startswith("lib"):
                            os.makedirs(join(build_dir, entry, libname, rel), exist_ok=True)


