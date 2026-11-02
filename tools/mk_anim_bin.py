#!/usr/bin/env python3
"""
AeroDENS Boot Animation Packer (mk_anim_bin.py)
-----------------------------------------------
Resizes an .mp4 video (e.g. `assets/anim/logo.mp4`), .gif animation, or .png
sequence to the GD32H759I-EVAL LCD resolution (`480 x 272`), extracts exact
per-frame presentation timestamps (`delay_ms`), and compresses all frames using
Delta-RLE565 into `build/anim.bin` + `include/anim_offsets.h` for the
`anim.bin` partition at 0x08200000 (1,792 KB max).
"""

import argparse
import binascii
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path
from PIL import Image, ImageDraw

PROJECT_ROOT = Path(__file__).resolve().parent.parent
LCD_WIDTH = 480
LCD_HEIGHT = 272
MAX_ANIM_PARTITION_BYTES = 1792 * 1024  # 1,792 KB at 0x08200000

ANIM_MAGIC = 0x4D494E41  # "ANIM" in little-endian
ANIM_VERSION = 1
FMT_RLE_RGB565 = 1
FMT_DELTA_RLE_RGB565 = 2


def pil_to_rgb565_pixels(im: Image.Image) -> list[int]:
    if im.mode in ("RGBA", "LA") or (im.mode == "P" and "transparency" in im.info):
        rgba = im.convert("RGBA")
        bg = Image.new("RGBA", rgba.size, (255, 255, 255, 255))
        im_rgb = Image.alpha_composite(bg, rgba).convert("RGB")
    else:
        im_rgb = im.convert("RGB")

    if im_rgb.size != (LCD_WIDTH, LCD_HEIGHT):
        im_rgb = im_rgb.resize((LCD_WIDTH, LCD_HEIGHT), Image.Resampling.LANCZOS)

    raw_rgb = im_rgb.tobytes()
    pixels = []
    for i in range(0, len(raw_rgb), 3):
        r = raw_rgb[i]
        g = raw_rgb[i + 1]
        b = raw_rgb[i + 2]
        # Snap H.264 near-white background (>246) to pure white (0xFFFF) for clean RLE
        if r >= 246 and g >= 246 and b >= 246:
            rgb565 = 0xFFFF
        else:
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        pixels.append(rgb565)
    return pixels


def encode_delta_rle_rgb565(pixels: list[int], prev_pixels: list[int] | None) -> bytes:
    """
    Delta-RLE565 encoding:
      - Word with bit 15 set (0x8000 | skip_len): skip `skip_len` (1..32767) unchanged pixels.
      - Word with bit 15 clear (run_len, rgb565_color): write `run_len` (1..32767) pixels of `rgb565_color`.
    """
    out = bytearray()
    n = len(pixels)
    i = 0
    while i < n:
        if prev_pixels is not None and pixels[i] == prev_pixels[i]:
            skip = 0
            while i < n and pixels[i] == prev_pixels[i] and skip < 0x7FFF:
                skip += 1
                i += 1
            if skip >= 2:
                out += struct.pack("<H", 0x8000 | skip)
                continue
            else:
                i -= skip

        c = pixels[i]
        run = 0
        while i < n and pixels[i] == c and run < 0x7FFF:
            if (
                prev_pixels is not None
                and run >= 1
                and i + 3 < n
                and pixels[i] == prev_pixels[i]
                and pixels[i + 1] == prev_pixels[i + 1]
                and pixels[i + 2] == prev_pixels[i + 2]
            ):
                break
            run += 1
            i += 1
        out += struct.pack("<HH", run, c)
    return bytes(out)


def generate_default_aerodens_frames(num_frames: int = 24) -> list[tuple[Image.Image, int]]:
    boot_png = PROJECT_ROOT / "assets" / "logo" / "boot.png"
    if boot_png.is_file():
        base_img = Image.open(boot_png).convert("RGB").resize((LCD_WIDTH, LCD_HEIGHT))
    else:
        base_img = Image.new("RGB", (LCD_WIDTH, LCD_HEIGHT), (255, 255, 255))

    frames = []
    bar_w = 220
    bar_h = 8
    bar_x0 = (LCD_WIDTH - bar_w) // 2
    bar_y0 = 168
    bar_x1 = bar_x0 + bar_w
    bar_y1 = bar_y0 + bar_h

    for idx in range(num_frames):
        frame = base_img.copy()
        draw = ImageDraw.Draw(frame)
        draw.rectangle([bar_x0 - 2, bar_y0 - 2, bar_x1 + 2, bar_y1 + 2], outline=(190, 190, 190), width=1)
        draw.rectangle([bar_x0, bar_y0, bar_x1, bar_y1], fill=(238, 238, 238))
        fill_w = int((bar_w * (idx + 1)) / num_frames)
        if fill_w > 0:
            draw.rectangle([bar_x0, bar_y0, bar_x0 + fill_w, bar_y1], fill=(128, 0, 24))
        frames.append((frame, 50))
    return frames


def load_frames_from_video_or_gif(
    media_path: Path, fallback_fps: int, max_frames: int
) -> list[tuple[Image.Image, int]]:
    default_delay = max(10, int(round(1000.0 / max(1, fallback_fps))))

    if media_path.suffix.lower() == ".gif":
        frames = []
        with Image.open(media_path) as im:
            frame_idx = 0
            while frame_idx < max_frames:
                im.seek(frame_idx)
                delay_ms = int(im.info.get("duration", default_delay)) or default_delay
                resized = im.convert("RGB").resize((LCD_WIDTH, LCD_HEIGHT), Image.Resampling.LANCZOS)
                frames.append((resized, max(10, min(5000, delay_ms))))
                frame_idx += 1
                try:
                    im.seek(frame_idx)
                except EOFError:
                    break
        return frames

    # Probe exact per-frame presentation timestamps (pts_time) via ffprobe
    pts_list = []
    try:
        pts_out = subprocess.check_output(
            [
                "ffprobe",
                "-v",
                "error",
                "-select_streams",
                "v:0",
                "-show_entries",
                "frame=pts_time",
                "-of",
                "csv=p=0",
                str(media_path),
            ],
            text=True,
        ).strip().splitlines()
        pts_list = [float(x.rstrip(",")) for x in pts_out if x.strip()]
    except Exception:
        pts_list = []

    tmpdir = Path(tempfile.mkdtemp(prefix="aerodens_anim_"))
    try:
        pattern = str(tmpdir / "frame_%04d.png")
        cmd = [
            "ffmpeg",
            "-y",
            "-i",
            str(media_path),
            "-vsync",
            "0",
            "-vf",
            f"scale={LCD_WIDTH}:{LCD_HEIGHT}:flags=lanczos",
            "-frames:v",
            str(max_frames),
            pattern,
        ]
        subprocess.check_call(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        pngs = sorted(tmpdir.glob("frame_*.png"))

        result = []
        for idx, p in enumerate(pngs):
            img = Image.open(p).convert("RGB")
            if idx + 1 < len(pts_list):
                dt_ms = int(round((pts_list[idx + 1] - pts_list[idx]) * 1000.0))
                delay_ms = max(15, min(5000, dt_ms))
            else:
                delay_ms = default_delay
            result.append((img, delay_ms))
        return result
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


def build_anim_bin(
    frames_with_delays: list[tuple[Image.Image, int]],
    nominal_fps: int,
    output_bin: Path,
    output_header: Path,
) -> None:
    encoded_frames = []
    delays_ms = []
    prev_pixels = None

    for im, delay_ms in frames_with_delays:
        pixels = pil_to_rgb565_pixels(im)
        rle = encode_delta_rle_rgb565(pixels, prev_pixels)
        prev_pixels = pixels
        encoded_frames.append(rle)
        delays_ms.append(delay_ms)

    # Header: 32 bytes
    # Directory table: frame_count * 12 bytes (uint32_t offset, uint32_t size, uint16_t delay_ms, uint16_t reserved)
    header_size = 32
    table_size = len(encoded_frames) * 12
    cur_offset = header_size + table_size

    frame_meta = []
    kept_payloads = []
    total_duration_ms = 0
    for idx, (payload, delay_ms) in enumerate(zip(encoded_frames, delays_ms)):
        if cur_offset + len(payload) > MAX_ANIM_PARTITION_BYTES:
            print(f"[WARN] Truncating animation at frame {idx} to fit 1,792 KB partition.")
            break
        frame_meta.append((cur_offset, len(payload), delay_ms))
        kept_payloads.append(payload)
        cur_offset += len(payload)
        total_duration_ms += delay_ms

    table_bytes = bytearray()
    for offset, size, delay_ms in frame_meta:
        table_bytes += struct.pack("<IIHH", offset, size, delay_ms, 0)

    payload_blob = b"".join(kept_payloads)
    if len(payload_blob) % 4 != 0:
        pad_len = 4 - (len(payload_blob) % 4)
        payload_blob += b"\xFF" * pad_len
        cur_offset += pad_len
    total_bytes = cur_offset
    crc32 = binascii.crc32(table_bytes + payload_blob) & 0xFFFFFFFF

    # 32-byte header:
    # uint32_t magic; uint16_t version; uint16_t frame_count;
    # uint16_t width; uint16_t height; uint16_t fps; uint16_t format;
    # uint32_t total_size; uint32_t crc32; uint32_t duration_ms; uint8_t reserved[4];
    header_bytes = struct.pack(
        "<IHHHHHHIII4s",
        ANIM_MAGIC,
        ANIM_VERSION,
        len(frame_meta),
        LCD_WIDTH,
        LCD_HEIGHT,
        nominal_fps,
        FMT_DELTA_RLE_RGB565,
        total_bytes,
        crc32,
        total_duration_ms,
        b"\x00" * 4,
    )

    output_bin.parent.mkdir(parents=True, exist_ok=True)
    output_bin.write_bytes(header_bytes + table_bytes + payload_blob)

    raw_equiv = len(frame_meta) * LCD_WIDTH * LCD_HEIGHT * 2
    total_kb = total_bytes / 1024.0
    total_mb = total_bytes / (1024.0 * 1024.0)
    ratio = raw_equiv / max(1, total_bytes)
    print("=" * 84)
    print(
        f" AeroDENS anim.bin Built -> {output_bin}\n"
        f" Resolution : {LCD_WIDTH} x {LCD_HEIGHT} RGB565 (Delta-RLE565 compressed)\n"
        f" Frames     : {len(frame_meta)} unique frames | Total Duration: {total_duration_ms/1000.0:.2f}s\n"
        f" Binary Size: {total_bytes:,} bytes ({total_kb:.2f} KB / {total_mb:.4f} MB — {ratio:.1f}x smaller than raw)"
    )
    print("=" * 84)

    write_anim_header(output_header, len(frame_meta), nominal_fps, total_bytes, total_duration_ms, crc32)
    print(f" Generated C Animation Header -> {output_header}")


def write_anim_header(
    header_path: Path, frame_count: int, fps: int, total_bytes: int, total_duration_ms: int, crc32: int
) -> None:
    lines = [
        "/* Auto-generated by tools/mk_anim_bin.py - DO NOT EDIT MANUALLY */",
        "#ifndef AERODENS_ANIM_OFFSETS_H",
        "#define AERODENS_ANIM_OFFSETS_H",
        "",
        "#include <stdint.h>",
        "#include <stddef.h>",
        "#include \"logo_offsets.h\"",
        "",
        "#define ANIM_PACK_MAGIC              0x4D494E41UL /* \"ANIM\" */",
        "#define ANIM_PACK_VERSION            1U",
        f"#define ANIM_PACK_FRAME_COUNT        {frame_count}U",
        f"#define ANIM_PACK_FPS                {fps}U",
        f"#define ANIM_PACK_DURATION_MS        {total_duration_ms}UL",
        f"#define ANIM_PACK_TOTAL_BYTES        {total_bytes}UL",
        f"#define ANIM_PACK_CRC32              0x{crc32:08X}UL",
        "",
        "#define ANIM_FMT_RLE_RGB565          1U",
        "#define ANIM_FMT_DELTA_RLE_RGB565    2U",
        "",
        "#ifndef ANIM_PARTITION_FLASH_ADDR",
        "#define ANIM_PARTITION_FLASH_ADDR     0x08200000UL",
        "#endif",
        "",
        "typedef struct {",
        "    uint32_t magic;        /* 0x4D494E41 (\"ANIM\") */",
        "    uint16_t version;      /* 1 */",
        "    uint16_t frame_count;  /* Total compressed frames */",
        "    uint16_t width;        /* 480 */",
        "    uint16_t height;       /* 272 */",
        "    uint16_t fps;          /* Nominal FPS */",
        "    uint16_t format;       /* 2 = DELTA_RLE565 */",
        "    uint32_t total_size;   /* Total size of anim.bin in bytes */",
        "    uint32_t crc32;        /* CRC32 of frame table + payloads */",
        "    uint32_t duration_ms;  /* Total playback duration in ms */",
        "    uint8_t  reserved[4];",
        "} __attribute__((packed)) anim_pack_header_t;",
        "",
        "typedef struct {",
        "    uint32_t offset;       /* Byte offset from start of anim.bin */",
        "    uint32_t size;         /* Compressed frame size in bytes */",
        "    uint16_t delay_ms;     /* Exact frame hold duration in ms */",
        "    uint16_t reserved;",
        "} __attribute__((packed)) anim_frame_entry_t;",
        "",
        "/* Decode a single Delta-RLE565 or RLE565 frame from anim.bin directly into dst_fb */",
        "static inline int anim_decode_frame(uintptr_t anim_base_addr, uint32_t offset, uint32_t size, uint16_t format, uint16_t *dst_fb, size_t max_pixels)",
        "{",
        "    const uint16_t *src = (const uint16_t *)(anim_base_addr + offset);",
        "    size_t words = size / 2U;",
        "    size_t i = 0;",
        "    size_t out_idx = 0;",
        "    if (format == ANIM_FMT_DELTA_RLE_RGB565) {",
        "        while (i < words && out_idx < max_pixels) {",
        "            uint16_t token = src[i++];",
        "            if ((token & 0x8000U) != 0U) {",
        "                out_idx += (size_t)(token & 0x7FFFU);",
        "            } else {",
        "                if (i >= words) break;",
        "                uint16_t color = src[i++];",
        "                uint16_t run_len = token;",
        "                while (run_len-- > 0U && out_idx < max_pixels) {",
        "                    dst_fb[out_idx++] = color;",
        "                }",
        "            }",
        "        }",
        "        return 0;",
        "    }",
        "    return logo_decode_from_offset(anim_base_addr, offset, size, (uint8_t)format, dst_fb, max_pixels);",
        "}",
        "",
        "#endif /* AERODENS_ANIM_OFFSETS_H */",
        "",
    ]
    header_path.parent.mkdir(parents=True, exist_ok=True)
    header_path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Compile .mp4 / .gif / .png sequence into build/anim.bin")
    parser.add_argument("inputs", nargs="*", type=Path, help="Input .mp4, .gif, or .png files")
    parser.add_argument("--fps", type=int, default=25, help="Nominal FPS (default: 25)")
    parser.add_argument("--max-frames", type=int, default=180, help="Max frames to extract (default: 180)")
    parser.add_argument("-o", "--output", type=Path, default=PROJECT_ROOT / "build" / "anim.bin")
    parser.add_argument("--header", type=Path, default=PROJECT_ROOT / "include" / "bootloader" / "anim_offsets.h")
    args = parser.parse_args()

    inputs = args.inputs
    if not inputs:
        anim_dir = PROJECT_ROOT / "assets" / "anim"
        if anim_dir.is_dir():
            mp4s = sorted(list(anim_dir.glob("*.mp4")) + list(anim_dir.glob("*.gif")))
            pngs = sorted(anim_dir.glob("*.png"))
            if mp4s:
                inputs = [mp4s[0]]
            elif pngs:
                inputs = pngs

    if inputs:
        if len(inputs) == 1 and inputs[0].suffix.lower() in (".mp4", ".gif", ".mov", ".mkv", ".webm"):
            frames_with_delays = load_frames_from_video_or_gif(inputs[0], args.fps, args.max_frames)
        else:
            default_delay = max(10, int(round(1000.0 / max(1, args.fps))))
            frames_with_delays = [
                (Image.open(p).convert("RGB"), default_delay) for p in inputs[: args.max_frames]
            ]
    else:
        frames_with_delays = generate_default_aerodens_frames(24)

    build_anim_bin(frames_with_delays, args.fps, args.output, args.header)
    return 0


if __name__ == "__main__":
    sys.exit(main())
