#!/usr/bin/env python3
"""
AeroDENS Boot Logo Packer (mk_logo_bin.py)
------------------------------------------
Converts multiple 480x272 PNG images into a single merged `logo.bin` container
(similar to Samsung's `up_param.bin`), records each image's exact byte offset
and size, and auto-generates a C header (`include/logo_offsets.h`) for the
bootloader.

Usage:
  python3 tools/mk_logo_bin.py                           # Packs assets/logo/boot.png & error.png (RLE565)
  python3 tools/mk_logo_bin.py --raw                     # Packs uncompressed RAW565 (516 KB)
  python3 tools/mk_logo_bin.py assets/logo/*.png         # Packs custom list of PNGs
"""

import argparse
import binascii
import re
import struct
import sys
from pathlib import Path
from PIL import Image

PROJECT_ROOT = Path(__file__).resolve().parent.parent
LCD_WIDTH = 480
LCD_HEIGHT = 272
SECTOR_ALIGN = 4096  # 0x1000 (4 KB GD32H7 FMC sector alignment)

LOGO_MAGIC = 0x4F474F4C  # "LOGO" in little-endian
LOGO_VERSION = 1

FMT_RAW_RGB565 = 0
FMT_RLE_RGB565 = 1


def align_up(value: int, alignment: int) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


def sanitize_c_identifier(filename: str) -> str:
    stem = Path(filename).stem.upper()
    ident = re.sub(r"[^A-Z0-9_]", "_", stem)
    if ident and ident[0].isdigit():
        ident = "_" + ident
    return ident or "IMAGE"


def image_to_rgb565_pixels(img_path: Path, width: int = LCD_WIDTH, height: int = LCD_HEIGHT) -> list[int]:
    """Load an image, composite alpha over white if RGBA, resize to 480x272, and convert to RGB565."""
    with Image.open(img_path) as im:
        if im.mode in ("RGBA", "LA") or (im.mode == "P" and "transparency" in im.info):
            rgba = im.convert("RGBA")
            bg = Image.new("RGBA", rgba.size, (255, 255, 255, 255))
            im_rgb = Image.alpha_composite(bg, rgba).convert("RGB")
        else:
            im_rgb = im.convert("RGB")

        if im_rgb.size != (width, height):
            im_rgb = im_rgb.resize((width, height), Image.Resampling.LANCZOS)

        raw_rgb = im_rgb.tobytes()
        pixels = []
        for i in range(0, len(raw_rgb), 3):
            r = raw_rgb[i]
            g = raw_rgb[i + 1]
            b = raw_rgb[i + 2]
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            pixels.append(rgb565)
        return pixels


def encode_raw_rgb565(pixels: list[int]) -> bytes:
    """Pack pixels as little-endian uint16_t array (261,120 bytes for 480x272)."""
    return struct.pack(f"<{len(pixels)}H", *pixels)


def encode_rle_rgb565(pixels: list[int]) -> bytes:
    """
    Pack pixels as simple 16-bit RLE pairs: (uint16_t count, uint16_t rgb565_color).
    Decodes in ~12 lines of C on Cortex-M7.
    """
    if not pixels:
        return b""
    out = bytearray()
    run_color = pixels[0]
    run_len = 1
    for px in pixels[1:]:
        if px == run_color and run_len < 0xFFFF:
            run_len += 1
        else:
            out += struct.pack("<HH", run_len, run_color)
            run_color = px
            run_len = 1
    out += struct.pack("<HH", run_len, run_color)
    return bytes(out)


def build_logo_bin(
    input_files: list[Path],
    output_bin: Path,
    output_header: Path | None,
    use_rle: bool = True,
    align_bytes: int = SECTOR_ALIGN,
) -> None:
    if not input_files:
        raise ValueError("No input PNG files provided.")

    entries = []
    payloads = []

    # Header is 32 bytes; each directory entry is 64 bytes
    header_size = 32
    dir_entry_size = 64
    data_start_offset = align_up(header_size + len(input_files) * dir_entry_size, align_bytes)

    current_offset = data_start_offset

    for idx, img_path in enumerate(input_files):
        if not img_path.is_file():
            raise FileNotFoundError(f"Input image not found: {img_path}")

        pixels = image_to_rgb565_pixels(img_path, LCD_WIDTH, LCD_HEIGHT)
        raw_bytes = encode_raw_rgb565(pixels)

        if use_rle:
            rle_bytes = encode_rle_rgb565(pixels)
            if len(rle_bytes) < len(raw_bytes):
                data_bytes = rle_bytes
                fmt = FMT_RLE_RGB565
            else:
                data_bytes = raw_bytes
                fmt = FMT_RAW_RGB565
        else:
            data_bytes = raw_bytes
            fmt = FMT_RAW_RGB565

        crc32 = binascii.crc32(data_bytes) & 0xFFFFFFFF
        name_str = img_path.stem[:31]
        c_ident = sanitize_c_identifier(img_path.name)

        entries.append(
            {
                "index": idx,
                "name": name_str,
                "c_ident": c_ident,
                "src_path": str(img_path),
                "offset": current_offset,
                "size": len(data_bytes),
                "raw_size": len(raw_bytes),
                "width": LCD_WIDTH,
                "height": LCD_HEIGHT,
                "format": fmt,
                "crc32": crc32,
            }
        )
        payloads.append(data_bytes)
        current_offset = align_up(current_offset + len(data_bytes), align_bytes)

    total_bin_size = current_offset
    blob = bytearray(b"\xFF" * total_bin_size)

    dir_table_bytes = bytearray()
    for e in entries:
        name_padded = e["name"].encode("ascii", errors="replace")[:31].ljust(32, b"\x00")
        entry_bin = struct.pack(
            "<32sIIHHB3sI12s",
            name_padded,
            e["offset"],
            e["size"],
            e["width"],
            e["height"],
            e["format"],
            b"\x00\x00\x00",
            e["crc32"],
            b"\x00" * 12,
        )
        dir_table_bytes += entry_bin

    dir_crc = binascii.crc32(dir_table_bytes) & 0xFFFFFFFF

    header_bin = struct.pack(
        "<IHHII16s",
        LOGO_MAGIC,
        LOGO_VERSION,
        len(entries),
        total_bin_size,
        dir_crc,
        b"\x00" * 16,
    )

    blob[0:header_size] = header_bin
    blob[header_size : header_size + len(dir_table_bytes)] = dir_table_bytes

    for e, data_bytes in zip(entries, payloads):
        start = e["offset"]
        blob[start : start + len(data_bytes)] = data_bytes

    output_bin.parent.mkdir(parents=True, exist_ok=True)
    output_bin.write_bytes(blob)

    total_mb = total_bin_size / (1024.0 * 1024.0)
    total_kb = total_bin_size / 1024.0
    print("=" * 88)
    print(
        f" AeroDENS logo.bin Built -> {output_bin}\n"
        f" Total Binary Size       : {total_bin_size:,} bytes ({total_kb:.2f} KB / {total_mb:.4f} MB)"
    )
    print("=" * 88)
    print(f"{'ID':>2}  {'Name':<18} {'Offset (Hex)':<14} {'Size (Bytes)':<14} {'Format':<10} {'CRC32':<10}")
    print("-" * 88)
    for e in entries:
        fmt_str = "RLE565" if e["format"] == FMT_RLE_RGB565 else "RAW565"
        print(
            f"{e['index']:>2}  {e['name']:<18} 0x{e['offset']:08X}     "
            f"{e['size']:<14,} {fmt_str:<10} 0x{e['crc32']:08X}"
        )
    print("=" * 88)

    if output_header is not None:
        output_header.parent.mkdir(parents=True, exist_ok=True)
        write_c_header(output_header, entries, total_bin_size)
        print(f" Generated C Offset Header -> {output_header}")


def write_c_header(header_path: Path, entries: list[dict], total_bin_size: int) -> None:
    lines = [
        "/* Auto-generated by tools/mk_logo_bin.py - DO NOT EDIT MANUALLY */",
        "#ifndef AERODENS_LOGO_OFFSETS_H",
        "#define AERODENS_LOGO_OFFSETS_H",
        "",
        "#include <stdint.h>",
        "#include <stddef.h>",
        "",
        "#define LOGO_PACK_MAGIC          0x4F474F4CUL /* \"LOGO\" */",
        "#define LOGO_PACK_VERSION        1U",
        f"#define LOGO_PACK_IMAGE_COUNT    {len(entries)}U",
        f"#define LOGO_PACK_TOTAL_BYTES    {total_bin_size}UL",
        "",
        "#define LOGO_FMT_RAW_RGB565      0U",
        "#define LOGO_FMT_RLE_RGB565      1U",
        "",
        "/* Default Internal Flash Partition Address for logo.bin (customizable) */",
        "#ifndef LOGO_PARTITION_FLASH_ADDR",
        "#define LOGO_PARTITION_FLASH_ADDR 0x08180000UL",
        "#endif",
        "",
        "/* Image Index Definitions */",
    ]
    for e in entries:
        lines.append(f"#define LOGO_ID_{e['c_ident']:<28} {e['index']}U")

    lines.append("")
    lines.append("/* Direct Byte Offsets, Sizes & Formats inside logo.bin */")
    for e in entries:
        lines.append(f"#define LOGO_OFFSET_{e['c_ident']:<24} 0x{e['offset']:08X}UL")
        lines.append(f"#define LOGO_SIZE_{e['c_ident']:<26} {e['size']}UL")
        lines.append(f"#define LOGO_FORMAT_{e['c_ident']:<24} {e['format']}U")
        lines.append(f"#define LOGO_CRC32_{e['c_ident']:<25} 0x{e['crc32']:08X}UL")

    lines.extend(
        [
            "",
            "typedef struct {",
            "    uint32_t magic;        /* 0x4F474F4C (\"LOGO\") */",
            "    uint16_t version;      /* 1 */",
            "    uint16_t image_count;  /* Number of packed images */",
            "    uint32_t total_size;   /* Total size of logo.bin in bytes */",
            "    uint32_t dir_crc32;    /* CRC32 of directory table */",
            "    uint8_t  reserved[16];",
            "} __attribute__((packed)) logo_pack_header_t;",
            "",
            "typedef struct {",
            "    char     name[32];     /* Null-terminated image tag */",
            "    uint32_t offset;       /* Byte offset from start of logo.bin */",
            "    uint32_t size;         /* Payload byte size */",
            "    uint16_t width;        /* 480 */",
            "    uint16_t height;       /* 272 */",
            "    uint8_t  format;       /* 0 = RAW RGB565, 1 = RLE RGB565 */",
            "    uint8_t  reserved[3];",
            "    uint32_t crc32;        /* Payload CRC32 */",
            "    uint8_t  pad[12];",
            "} __attribute__((packed)) logo_pack_entry_t;",
            "",
            "/* Decode a packed image from logo.bin (RAW565 or RLE565) directly into a 480x272 RGB565 framebuffer */",
            "static inline int logo_decode_from_offset(uintptr_t logo_base_addr, uint32_t offset, uint32_t size, uint8_t format, uint16_t *dst_fb, size_t max_pixels)",
            "{",
            "    const uint16_t *src = (const uint16_t *)(logo_base_addr + offset);",
            "    if (format == LOGO_FMT_RAW_RGB565) {",
            "        size_t count = size / 2U;",
            "        if (count > max_pixels) count = max_pixels;",
            "        for (size_t i = 0; i < count; ++i) {",
            "            dst_fb[i] = src[i];",
            "        }",
            "        return 0;",
            "    } else if (format == LOGO_FMT_RLE_RGB565) {",
            "        size_t words = size / 2U;",
            "        size_t out_idx = 0;",
            "        for (size_t i = 0; i + 1 < words && out_idx < max_pixels; i += 2) {",
            "            uint16_t run_len = src[i];",
            "            uint16_t color   = src[i + 1];",
            "            while (run_len-- > 0U && out_idx < max_pixels) {",
            "                dst_fb[out_idx++] = color;",
            "            }",
            "        }",
            "        return 0;",
            "    }",
            "    return -1;",
            "}",
            "",
            "/* Render an image by its directory index (0 = boot, 1 = error, etc.) from logo.bin in Flash */",
            "static inline int logo_decode_by_id(uintptr_t logo_base_addr, uint32_t image_id, uint16_t *dst_fb, size_t max_pixels)",
            "{",
            "    const logo_pack_header_t *hdr = (const logo_pack_header_t *)logo_base_addr;",
            "    if (hdr->magic != LOGO_PACK_MAGIC || image_id >= hdr->image_count) {",
            "        return -1;",
            "    }",
            "    const logo_pack_entry_t *entries = (const logo_pack_entry_t *)(logo_base_addr + sizeof(logo_pack_header_t));",
            "    const logo_pack_entry_t *e = &entries[image_id];",
            "    return logo_decode_from_offset(logo_base_addr, e->offset, e->size, e->format, dst_fb, max_pixels);",
            "}",
            "",
            "#endif /* AERODENS_LOGO_OFFSETS_H */",
            "",
        ]
    )
    header_path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Pack 480x272 PNG images into an up_param-style logo.bin with recorded offsets."
    )
    parser.add_argument(
        "inputs",
        nargs="*",
        type=Path,
        help="Input PNG files (defaults to assets/logo/boot.png and assets/logo/error.png)",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=PROJECT_ROOT / "build" / "logo.bin",
        help="Output merged binary path (default: build/logo.bin)",
    )
    parser.add_argument(
        "--header",
        type=Path,
        default=PROJECT_ROOT / "include" / "bootloader" / "logo_offsets.h",
        help="Output C header path with recorded offsets (default: include/bootloader/logo_offsets.h)",
    )
    parser.add_argument(
        "--raw",
        action="store_true",
        help="Store uncompressed RAW565 (261,120 bytes per image) instead of lossless RLE565",
    )
    parser.add_argument(
        "--align",
        type=int,
        default=SECTOR_ALIGN,
        help="Byte alignment for each image offset (default: 4096)",
    )

    args = parser.parse_args()
    inputs = args.inputs
    if not inputs:
        default_dir = PROJECT_ROOT / "assets" / "logo"
        inputs = [default_dir / "boot.png", default_dir / "error.png"]

    build_logo_bin(
        input_files=inputs,
        output_bin=args.output,
        output_header=args.header,
        use_rle=not args.raw,
        align_bytes=args.align,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
