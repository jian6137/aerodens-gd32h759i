#!/usr/bin/env python3
"""
Convert assets/icons/*.png and *.svg to LVGL C image descriptors in src/os/ui/.
Color format: LV_IMG_CF_TRUE_COLOR_ALPHA (RGB565 + 8-bit Alpha per pixel, 3 bytes/pixel).
"""

import os
import subprocess
import sys
from PIL import Image, ImageDraw, ImageFilter


def write_c_array(out_c_path: str, out_h_path: str, var_name: str, width: int, height: int, pixels, has_alpha: bool = True):
    map_name = f"{var_name}_map"
    header_guard = f"{var_name.upper()}_H"

    # Header
    with open(out_h_path, "w", encoding="utf-8") as f:
        f.write(f'/*!\n'
                f'    \\file    {os.path.basename(out_h_path)}\n'
                f'    \\brief   LVGL Image Descriptor for {var_name}\n'
                f'*/\n\n'
                f'#ifndef {header_guard}\n'
                f'#define {header_guard}\n\n'
                f'#include "lvgl.h"\n\n'
                f'#ifdef __cplusplus\nextern "C" {{\n#endif\n\n'
                f'extern const lv_img_dsc_t {var_name};\n\n'
                f'#ifdef __cplusplus\n}}\n#endif\n\n'
                f'#endif /* {header_guard} */\n')

    # C Source
    bytes_per_pixel = 3 if has_alpha else 2
    cf_type = "LV_IMG_CF_TRUE_COLOR_ALPHA" if has_alpha else "LV_IMG_CF_TRUE_COLOR"

    with open(out_c_path, "w", encoding="utf-8") as f:
        f.write(f'/*!\n'
                f'    \\file    {os.path.basename(out_c_path)}\n'
                f'    \\brief   LVGL Image Descriptor Implementation for {var_name}\n'
                f'*/\n\n'
                f'#include "{os.path.basename(out_h_path)}"\n\n'
                f'#ifndef LV_ATTRIBUTE_MEM_ALIGN\n'
                f'#define LV_ATTRIBUTE_MEM_ALIGN __attribute__((aligned(4)))\n'
                f'#endif\n\n'
                f'const LV_ATTRIBUTE_MEM_ALIGN uint8_t {map_name}[] = {{\n')

        line_count = 0
        f.write("    ")
        for p in pixels:
            if has_alpha:
                r, g, b, a = p
                if a == 0:
                    c_low, c_high, alpha = 0, 0, 0
                else:
                    r5 = (r >> 3) & 0x1F
                    g6 = (g >> 2) & 0x3F
                    b5 = (b >> 3) & 0x1F
                    rgb565 = (r5 << 11) | (g6 << 5) | b5
                    c_low = rgb565 & 0xFF
                    c_high = (rgb565 >> 8) & 0xFF
                    alpha = a
                f.write(f"0x{c_low:02X}, 0x{c_high:02X}, 0x{alpha:02X}, ")
            else:
                r, g, b = p[:3]
                r5 = (r >> 3) & 0x1F
                g6 = (g >> 2) & 0x3F
                b5 = (b >> 3) & 0x1F
                rgb565 = (r5 << 11) | (g6 << 5) | b5
                c_low = rgb565 & 0xFF
                c_high = (rgb565 >> 8) & 0xFF
                f.write(f"0x{c_low:02X}, 0x{c_high:02X}, ")

            line_count += 1
            if line_count >= 16:
                f.write("\n    ")
                line_count = 0
        f.write("\n")

        f.write('};\n\n'
                f'const lv_img_dsc_t {var_name} = {{\n'
                f'    .header.always_zero = 0,\n'
                f'    .header.w = {width},\n'
                f'    .header.h = {height},\n'
                f'    .data_size = {width * height * bytes_per_pixel},\n'
                f'    .header.cf = {cf_type},\n'
                f'    .data = {map_name},\n'
                f'}};\n')

    print(f"[mk_icon_c] Generated {out_c_path} ({width}x{height}, {width * height * bytes_per_pixel} bytes)")


def generate_os_icon(project_dir: str):
    svg_path = os.path.join(project_dir, "assets", "icons", "ic_os_white.svg")
    out_c_path = os.path.join(project_dir, "src", "os", "ui", "icons", "img_ic_os_white.c")
    out_h_path = os.path.join(project_dir, "src", "os", "ui", "icons", "img_ic_os_white.h")
    tmp_png = "/tmp/ic_os_icon_tmp.png"

    if not os.path.isfile(svg_path):
        return

    subprocess.run([
        "convert", "-background", "none", "-density", "300",
        svg_path, "-resize", "20x22", tmp_png
    ], check=True)

    img = Image.open(tmp_png).convert("RGBA")
    w, h = img.size
    pixels = list(img.getdata())
    write_c_array(out_c_path, out_h_path, "img_ic_os_white", w, h, pixels)


def generate_circle_airline_icon(png_path: str, out_c_path: str, out_h_path: str, var_name: str, size: int = 36):
    if not os.path.isfile(png_path):
        return

    img = Image.open(png_path).convert("RGBA")
    img = img.resize((size, size), Image.Resampling.LANCZOS)

    # 4x supersampled smooth circular antialiased mask
    scale = 4
    mask_large = Image.new("L", (size * scale, size * scale), 0)
    draw = ImageDraw.Draw(mask_large)
    draw.ellipse((0, 0, size * scale - 1, size * scale - 1), fill=255)
    mask = mask_large.resize((size, size), Image.Resampling.LANCZOS)

    r, g, b, a = img.split()
    a_bytes = bytes(min(a_val, m_val) for a_val, m_val in zip(a.tobytes(), mask.tobytes()))
    a_smooth = Image.frombytes("L", (size, size), a_bytes)
    final_img = Image.merge("RGBA", (r, g, b, a_smooth))

    w, h = final_img.size
    pixels = list(final_img.getdata())
    write_c_array(out_c_path, out_h_path, var_name, w, h, pixels)


def generate_aircraft_silhouette(svg_path: str, out_c_path: str, out_h_path: str, var_name: str, width: int = 120, height: int = 132):
    if not os.path.isfile(svg_path):
        return

    tmp_png = f"/tmp/{var_name}_hires.png"
    cmd = ["magick", "-background", "none", "-density", "600", svg_path, tmp_png]
    try:
        subprocess.run(cmd, check=True)
    except FileNotFoundError:
        cmd[0] = "convert"
        subprocess.run(cmd, check=True)

    img = Image.open(tmp_png).convert("RGBA")
    bbox = img.getbbox()
    if bbox:
        img = img.crop(bbox)

    # Dilate alpha slightly at hi-res so thin vector strokes survive downsampling crisply
    r, g, b, a = img.split()
    a = a.filter(ImageFilter.MaxFilter(5))
    img = Image.merge("RGBA", (r, g, b, a))

    img = img.resize((width, height), Image.Resampling.LANCZOS)
    r, g, b, a = img.split()
    a = a.point(lambda v: min(255, int(v * 1.8)))
    white = Image.new("L", img.size, 255)
    img = Image.merge("RGBA", (white, white, white, a))

    w, h = img.size
    pixels = list(img.getdata())
    write_c_array(out_c_path, out_h_path, var_name, w, h, pixels)


def generate_flight_separator(project_dir: str):
    svg_path = os.path.join(project_dir, "assets", "icons", "ic_flight_separator.svg")
    out_c_path = os.path.join(project_dir, "src", "os", "ui", "icons", "img_ic_flight_separator.c")
    out_h_path = os.path.join(project_dir, "src", "os", "ui", "icons", "img_ic_flight_separator.h")
    tmp_png = "/tmp/ic_flight_separator_tmp.png"

    if not os.path.isfile(svg_path):
        return

    cmd = ["magick", "-background", "none", "-density", "300", svg_path, "-resize", "238x24", tmp_png]
    try:
        subprocess.run(cmd, check=True)
    except FileNotFoundError:
        cmd[0] = "convert"
        subprocess.run(cmd, check=True)

    img = Image.open(tmp_png).convert("RGBA")
    r, g, b, a = img.split()
    a = a.point(lambda v: min(255, int(v * 1.4)))
    img = Image.merge("RGBA", (r, g, b, a))

    w, h = img.size
    pixels = list(img.getdata())
    write_c_array(out_c_path, out_h_path, "img_ic_flight_separator", w, h, pixels)


def generate_svg_icon(project_dir: str, svg_name: str, var_name: str, size: int = 48):
    svg_path = os.path.join(project_dir, "assets", "icons", svg_name)
    out_c_path = os.path.join(project_dir, "src", "os", "ui", "icons", f"{var_name}.c")
    out_h_path = os.path.join(project_dir, "src", "os", "ui", "icons", f"{var_name}.h")
    tmp_png = f"/tmp/{var_name}_tmp.png"

    if not os.path.isfile(svg_path):
        return

    cmd = [
        "magick", "-background", "none", "-density", "300",
        svg_path, "-resize", f"{size}x{size}", tmp_png
    ]
    try:
        subprocess.run(cmd, check=True)
    except FileNotFoundError:
        cmd[0] = "convert"
        subprocess.run(cmd, check=True)

    img = Image.open(tmp_png).convert("RGBA")
    w, h = img.size
    pixels = list(img.getdata())
    write_c_array(out_c_path, out_h_path, var_name, w, h, pixels)


def generate_connect_icon(project_dir: str):
    generate_svg_icon(project_dir, "ic_connect.svg", "img_ic_connect", 48)


def generate_sdcard_icon(project_dir: str):
    generate_svg_icon(project_dir, "ic_sdcard.svg", "img_ic_sdcard", 48)


def generate_empty_icon(project_dir: str):
    generate_svg_icon(project_dir, "ic_empty.svg", "img_ic_empty", 48)


def generate_wallpaper(project_dir: str):
    png_path = os.path.join(project_dir, "assets", "icons", "wallpaper.png")
    out_c_path = os.path.join(project_dir, "src", "os", "ui", "icons", "img_wallpaper.c")
    out_h_path = os.path.join(project_dir, "src", "os", "ui", "icons", "img_wallpaper.h")

    if not os.path.isfile(png_path):
        return

    img = Image.open(png_path).convert("RGB")
    w, h = img.size
    pixels = list(img.getdata())
    write_c_array(out_c_path, out_h_path, "img_wallpaper", w, h, pixels, has_alpha=False)


def generate_all(project_dir: str):
    icons_dir = os.path.join(project_dir, "src", "os", "ui", "icons")
    os.makedirs(icons_dir, exist_ok=True)

    # 1. OS Logo Icon (20x22)
    generate_os_icon(project_dir)

    # 2. Cebu Pacific Airline Icon (36x36 Small & 72x72 Large Circle)
    path_5j = os.path.join(project_dir, "assets", "icons", "5J.png")
    out_c_5j = os.path.join(icons_dir, "img_ic_airline_5j.c")
    out_h_5j = os.path.join(icons_dir, "img_ic_airline_5j.h")
    generate_circle_airline_icon(path_5j, out_c_5j, out_h_5j, "img_ic_airline_5j", 36)

    out_c_5j_lg = os.path.join(icons_dir, "img_ic_airline_5j_lg.c")
    out_h_5j_lg = os.path.join(icons_dir, "img_ic_airline_5j_lg.h")
    generate_circle_airline_icon(path_5j, out_c_5j_lg, out_h_5j_lg, "img_ic_airline_5j_lg", 72)

    # 3. Thai Airways Airline Icon (36x36 Small & 72x72 Large Circle)
    path_tg = os.path.join(project_dir, "assets", "icons", "TG.png")
    out_c_tg = os.path.join(icons_dir, "img_ic_airline_tg.c")
    out_h_tg = os.path.join(icons_dir, "img_ic_airline_tg.h")
    generate_circle_airline_icon(path_tg, out_c_tg, out_h_tg, "img_ic_airline_tg", 36)

    out_c_tg_lg = os.path.join(icons_dir, "img_ic_airline_tg_lg.c")
    out_h_tg_lg = os.path.join(icons_dir, "img_ic_airline_tg_lg.h")
    generate_circle_airline_icon(path_tg, out_c_tg_lg, out_h_tg_lg, "img_ic_airline_tg_lg", 72)

    # 4. A320 Silhouette (120x132, tightly cropped)
    path_a320 = os.path.join(project_dir, "assets", "icons", "a320.svg")
    if not os.path.isfile(path_a320):
        path_a320 = os.path.join(project_dir, "assets", "icons", "A320.svg")
    out_c_a320 = os.path.join(icons_dir, "img_ac_a320.c")
    out_h_a320 = os.path.join(icons_dir, "img_ac_a320.h")
    generate_aircraft_silhouette(path_a320, out_c_a320, out_h_a320, "img_ac_a320", 120, 132)

    # 5. A330 Silhouette (120x132, tightly cropped)
    path_a330 = os.path.join(project_dir, "assets", "icons", "a330.svg")
    if not os.path.isfile(path_a330):
        path_a330 = os.path.join(project_dir, "assets", "icons", "A330.svg")
    out_c_a330 = os.path.join(icons_dir, "img_ac_a330.c")
    out_h_a330 = os.path.join(icons_dir, "img_ac_a330.h")
    generate_aircraft_silhouette(path_a330, out_c_a330, out_h_a330, "img_ac_a330", 120, 132)

    # 6. Flight Route Separator SVG (238x24)
    generate_flight_separator(project_dir)

    # 7. Fullscreen Wallpaper (480x272 RGB565)
    generate_wallpaper(project_dir)

    # 8. Hardware Setup Connect Icon (48x48 RGBA)
    generate_connect_icon(project_dir)

    # 9. SD Card Icon (48x48 RGBA)
    generate_sdcard_icon(project_dir)

    # 10. Empty Folder / Nothing Here Icon (48x48 RGBA)
    generate_empty_icon(project_dir)


if __name__ == "__main__":
    pdir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    generate_all(pdir)
