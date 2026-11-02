"""
PlatformIO Post-Build Hook (tools/pio_post_partitions.py)
---------------------------------------------------------
1. Resolves project-local linker scripts (`ldscripts/gd32h759i_bootloader.ld`,
   `ldscripts/gd32h759i_os.ld`).
2. Overrides OpenOCD flash address when `board_upload.offset_address` is set
   (so `[env:os]` flashes directly to `0x08010000`).
3. Registers custom PlatformIO targets:
   - `upload_logo` : Flash `build/logo.bin` to `0x08180000`
   - `upload_anim` : Flash `build/anim.bin` to `0x08200000`
"""

import os
from os.path import isfile, join

Import("env")

project_dir = env.subst("$PROJECT_DIR")
board = env.BoardConfig()
platform = env.PioPlatform()

# 1. Ensure project-relative ldscript path resolves cleanly
ldscript = board.get("build.ldscript", "")
if ldscript:
    proj_ld = join(project_dir, ldscript)
    if isfile(proj_ld):
        env.Replace(LDSCRIPT_PATH=proj_ld)

# 2. Honor board_upload.offset_address (e.g. 0x08010000 for [env:os])
offset_addr = board.get("upload.offset_address", "0x08000000")
if offset_addr and offset_addr != "0x08000000":
    flags = env.get("UPLOADERFLAGS", [])
    updated_flags = [
        f.replace("0x08000000", str(offset_addr)) if isinstance(f, str) else f
        for f in flags
    ]
    env.Replace(UPLOADERFLAGS=updated_flags)

# 3. Register custom partition flash targets (`upload_logo` and `upload_anim`)
openocd_dir = platform.get_package_dir("tool-openocd") or ""
openocd_bin = join(openocd_dir, "bin", "openocd") if openocd_dir else "openocd"
openocd_scripts = join(openocd_dir, "openocd", "scripts") if openocd_dir else ""
board_openocd_dir = join(platform.get_dir(), "misc", "openocd")

logo_bin_path = join(project_dir, "build", "logo.bin")
anim_bin_path = join(project_dir, "build", "anim.bin")


def make_openocd_flash_cmd(bin_path: str, flash_addr: str) -> str:
    return (
        f'"{openocd_bin}" '
        f'-s "{board_openocd_dir}" '
        + (f'-s "{openocd_scripts}" ' if openocd_scripts else "")
        + "-f interface/cmsis-dap.cfg "
        + '-c "cmsis_dap_vid_pid 0x28e9 0x0497 0x28e9 0x058f 0x28e9 0x018a" '
        + '-c "transport select swd" '
        + "-f gd32h7x.cfg "
        + f'-c "gd32h7_flash_bin {{{bin_path}}} {flash_addr}"'
    )


def make_openocd_erase_cmd(flash_addr: str) -> str:
    # Avoid '$' variables so neither SCons nor bash strips them
    tcl_cmd = (
        f"init; reset halt; sleep 10; "
        f"echo {{GD32H7 FMC: Erasing sector at {flash_addr}...}}; "
        f"gd32h7_fmc_wait_ready; "
        f"mww 0x52002004 0x45670123; "
        f"mww 0x52002004 0xCDEF89AB; "
        f"mww 0x52002010 0x47870000; "
        f"mww 0x5200200C 0x00000004; "
        f"mww 0x52002014 {flash_addr}; "
        f"mww 0x5200200C 0x00000084; "
        f"gd32h7_fmc_wait_ready; "
        f"mww 0x5200200C 0x00000000; "
        f"mww 0x5200200C 0x00000001; "
        f"echo {{GD32H7 FMC: Erase complete, resetting target...}}; "
        f"reset run; shutdown"
    )
    return (
        f'"{openocd_bin}" '
        f'-s "{board_openocd_dir}" '
        + (f'-s "{openocd_scripts}" ' if openocd_scripts else "")
        + "-f interface/cmsis-dap.cfg "
        + '-c "cmsis_dap_vid_pid 0x28e9 0x0497 0x28e9 0x058f 0x28e9 0x018a" '
        + '-c "transport select swd" '
        + "-f gd32h7x.cfg "
        + f'-c "{tcl_cmd}"'
    )


env.AddCustomTarget(
    name="upload_logo",
    dependencies=None,
    actions=[make_openocd_flash_cmd(logo_bin_path, "0x08180000")],
    title="Upload logo.bin to 0x08180000",
    description="Flash AeroDENS build/logo.bin container to 0x08180000",
)

env.AddCustomTarget(
    name="upload_anim",
    dependencies=None,
    actions=[make_openocd_flash_cmd(anim_bin_path, "0x08200000")],
    title="Upload anim.bin to 0x08200000",
    description="Flash AeroDENS build/anim.bin boot animation to 0x08200000",
)

env.AddCustomTarget(
    name="erase_os",
    dependencies=None,
    actions=[make_openocd_erase_cmd("0x08010000")],
    title="Erase OS Partition (0x08010000)",
    description="Clear the OS partition vector table at 0x08010000 to test bootloader error screen",
)


