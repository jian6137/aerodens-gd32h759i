#!/usr/bin/env python3
"""
AeroDENS Samsung Odin-Style Firmware Flasher (tools/flasher/flasher.py)
----------------------------------------------------------------------
Dual-mode GUI & CLI firmware flashing tool for GigaDevice GD32H759.
Works cross-platform on both Linux and Windows in authentic Light Mode.

Layout:
- Matches Samsung Odin v3.07 layout with top banner, PASS! ID:COM box,
  Option frame, Message live console, and a vertical LIST of partitions
  under 'Files [Download]', each with checkbox, button, text entry,
  and progress bar (0% all initially).
- Multi-language: Supports English and 简体中文 (no .ini mentioned in UI).
"""

import argparse
import binascii
import configparser
import glob
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import threading
import time
from pathlib import Path
from typing import Callable, Dict, List, Optional, Tuple

# Base paths
SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent.parent
LANG_DIR = SCRIPT_DIR / "lang"
LOGO_SVG = SCRIPT_DIR / "ic_logo.svg"
LOGO_PNG = SCRIPT_DIR / "ic_logo.png"

# Default partition layout matching build-all & flashinfo.json
DEFAULT_PARTITIONS = [
    {
        "id": "bootloader",
        "name_key": "bootloader",
        "btn_label": "Bootloader",
        "filename": "bootloader.bin",
        "addr": 0x08000000,
        "max_size": 64 * 1024,
    },
    {
        "id": "os",
        "name_key": "os",
        "btn_label": "OS (APP)",
        "filename": "os.bin",
        "addr": 0x08010000,
        "max_size": 1472 * 1024,
    },
    {
        "id": "logo",
        "name_key": "logo",
        "btn_label": "LOGO",
        "filename": "logo.bin",
        "addr": 0x08180000,
        "max_size": 512 * 1024,
    },
    {
        "id": "anim",
        "name_key": "anim",
        "btn_label": "ANIMATION",
        "filename": "anim.bin",
        "addr": 0x08200000,
        "max_size": 1792 * 1024,
    },
]

# GigaDevice USB Vendor IDs
GD32_VIDS = ["28e9"]


# ==============================================================================
# Internationalization / Localization (I18n)
# ==============================================================================
class I18n:
    def __init__(self, lang_code: str = "en-us"):
        self.lang_code = lang_code
        self.config = configparser.ConfigParser(interpolation=None)
        self.load(lang_code)

    def load(self, lang_code: str) -> None:
        ini_file = LANG_DIR / f"{lang_code}.ini"
        if not ini_file.is_file():
            ini_file = LANG_DIR / "en-us.ini"
            lang_code = "en-us"

        self.lang_code = lang_code
        self.config = configparser.ConfigParser(interpolation=None)
        if ini_file.is_file():
            self.config.read(str(ini_file), encoding="utf-8")

    def get(self, section: str, key: str, fallback: str = "") -> str:
        try:
            return self.config.get(section, key, fallback=fallback)
        except Exception:
            return fallback

    def fmt(self, section: str, key: str, **kwargs) -> str:
        text = self.get(section, key, "")
        try:
            return text.format(**kwargs)
        except Exception:
            return text


I18N = I18n("en-us")


# ==============================================================================
# Device Detection & OpenOCD Probing
# ==============================================================================
class DeviceProbe:
    """Detects and tests GD32 debug probes (GDLink, CMSIS-DAP) on Linux & Windows."""

    @staticmethod
    def find_openocd() -> Optional[str]:
        candidates = [
            PROJECT_ROOT / ".pio" / "core" / "packages" / "tool-openocd" / "bin" / ("openocd.exe" if sys.platform == "win32" else "openocd"),
            Path.home() / ".platformio" / "packages" / "tool-openocd" / "bin" / ("openocd.exe" if sys.platform == "win32" else "openocd"),
        ]
        for c in candidates:
            if c.is_file() and os.access(str(c), os.X_OK):
                return str(c)

        exe_name = "openocd.exe" if sys.platform == "win32" else "openocd"
        w = shutil.which(exe_name)
        if w:
            return w

        return None

    @staticmethod
    def get_openocd_search_paths() -> List[str]:
        paths = []
        proj_openocd = PROJECT_ROOT / "tools" / "openocd"
        if proj_openocd.is_dir():
            paths.append(str(proj_openocd))

        board_openocd = PROJECT_ROOT / ".pio" / "core" / "platforms" / "gd32h7" / "misc" / "openocd"
        if board_openocd.is_dir():
            paths.append(str(board_openocd))

        openocd_exe = DeviceProbe.find_openocd()
        if openocd_exe:
            scripts_dir = Path(openocd_exe).parent.parent / "openocd" / "scripts"
            if scripts_dir.is_dir():
                paths.append(str(scripts_dir))

        return paths

    @staticmethod
    def detect_devices() -> List[Dict[str, str]]:
        devices = []

        # Linux USB sysfs enumeration
        if sys.platform.startswith("linux"):
            for p in glob.glob("/sys/bus/usb/devices/*"):
                id_vendor_f = os.path.join(p, "idVendor")
                if os.path.isfile(id_vendor_f):
                    try:
                        with open(id_vendor_f, "r") as f:
                            vid = f.read().strip().lower()
                        if vid in GD32_VIDS:
                            def read_attr(name):
                                fp = os.path.join(p, name)
                                return open(fp).read().strip() if os.path.isfile(fp) else ""

                            pid = read_attr("idProduct").lower()
                            prod = read_attr("product") or "GDLinker_V2"
                            mfg = read_attr("manufacturer") or "GigaDevice"
                            sn = read_attr("serial") or "N/A"
                            bus = os.path.basename(p)
                            devices.append({
                                "name": f"{mfg} {prod}",
                                "vid": f"0x{vid}",
                                "pid": f"0x{pid}",
                                "serial": sn,
                                "bus": bus,
                                "desc": f"{mfg} {prod} (VID:0x{vid} PID:0x{pid} S/N:{sn})"
                            })
                    except Exception:
                        pass

        # Windows USB detection via PowerShell
        elif sys.platform == "win32":
            try:
                cmd = ["powershell", "-NoProfile", "-Command",
                       "Get-PnpDevice -PresentOnly | Where-Object { $_.InstanceId -like '*VID_28E9*' } | "
                       "Select-Object -Property FriendlyName, InstanceId | ConvertTo-Json"]
                res = subprocess.run(cmd, capture_output=True, text=True, timeout=5)
                if res.returncode == 0 and res.stdout.strip():
                    data = json.loads(res.stdout)
                    if isinstance(data, dict):
                        data = [data]
                    for item in data:
                        fname = item.get("FriendlyName") or "GD-Link Probe"
                        inst = item.get("InstanceId", "")
                        match = re.search(r"VID_([0-9A-Fa-f]{4})&PID_([0-9A-Fa-f]{4})\\([0-9A-Za-z_-]+)", inst)
                        if match:
                            vid, pid, sn = match.group(1).lower(), match.group(2).lower(), match.group(3)
                        else:
                            vid, pid, sn = "28e9", "0497", "N/A"
                        devices.append({
                            "name": fname,
                            "vid": f"0x{vid}",
                            "pid": f"0x{pid}",
                            "serial": sn,
                            "bus": "USB",
                            "desc": f"{fname} (VID:0x{vid} PID:0x{pid} S/N:{sn})"
                        })
            except Exception:
                pass

        if not devices:
            openocd = DeviceProbe.find_openocd()
            if openocd:
                devices.append({
                    "name": "GDMicroelectronics GDLinker_V2 (Auto-Detect)",
                    "vid": "0x28e9",
                    "pid": "0x0497",
                    "serial": "Auto",
                    "bus": "CMSIS-DAP",
                    "desc": "GD-Link CMSIS-DAP Debug Probe (Auto-Detect)"
                })

        return devices

    @staticmethod
    def test_connection(device: Optional[Dict[str, str]] = None) -> Tuple[bool, str]:
        openocd = DeviceProbe.find_openocd()
        if not openocd:
            return False, I18N.get("log", "openocd_not_found", "OpenOCD executable not found!")

        cmd = [openocd]
        for sp in DeviceProbe.get_openocd_search_paths():
            cmd.extend(["-s", sp])

        cmd.extend([
            "-f", "interface/cmsis-dap.cfg",
            "-c", "cmsis_dap_vid_pid 0x28e9 0x0497 0x28e9 0x058f 0x28e9 0x018a",
            "-c", "transport select swd",
            "-f", "gd32h7x.cfg",
            "-c", "init; targets; shutdown",
        ])

        try:
            res = subprocess.run(cmd, capture_output=True, text=True, timeout=8)
            out = res.stdout + "\n" + res.stderr
            if res.returncode == 0 and ("Cortex-M7" in out or "gd32h7x" in out or "target halted" in out or "DPIDR" in out):
                return True, I18N.get("connection", "status_ok", "Connection established successfully!")
            else:
                lines = [l for l in out.splitlines() if "Error:" in l or "Warn :" in l or "failed" in l]
                err_detail = lines[-1] if lines else "Target probe did not respond."
                return False, f"{I18N.get('connection', 'status_fail', 'Connection failed!')} ({err_detail})"
        except subprocess.TimeoutExpired:
            return False, "OpenOCD connection test timed out."
        except Exception as e:
            return False, f"Connection test error: {e}"


# ==============================================================================
# Firmware Package (TAR) Parser & Validator
# ==============================================================================
class FirmwarePackage:
    """Inspects and validates an AeroDENS .tar firmware archive."""

    def __init__(self, tar_path: str):
        self.tar_path = Path(tar_path)
        self.is_valid = False
        self.error_msg = ""
        self.manifest: Dict = {}
        self.partitions: List[Dict] = []
        self.temp_dir: Optional[tempfile.TemporaryDirectory] = None

    def validate(self) -> Tuple[bool, str]:
        if not self.tar_path.is_file():
            self.error_msg = f"File not found: {self.tar_path}"
            return False, self.error_msg

        if not tarfile.is_tarfile(str(self.tar_path)):
            self.error_msg = I18N.get("firmware", "err_not_tar", "Selected file is not a valid TAR archive!")
            return False, self.error_msg

        try:
            with tarfile.open(str(self.tar_path), "r") as tar:
                names = tar.getnames()

                if "flashinfo.json" in names:
                    f = tar.extractfile("flashinfo.json")
                    if f:
                        self.manifest = json.loads(f.read().decode("utf-8"))

                manifest_parts = self.manifest.get("partitions", [])
                part_map = {p["name"]: p for p in manifest_parts}

                self.partitions = []
                for spec in DEFAULT_PARTITIONS:
                    p_id = spec["id"]
                    fn = spec["filename"]
                    if fn not in names:
                        self.error_msg = f"{I18N.get('firmware', 'err_missing_file', 'Missing partition file:')} {fn}"
                        return False, self.error_msg

                    f = tar.extractfile(fn)
                    if not f:
                        self.error_msg = f"Cannot read stream for: {fn}"
                        return False, self.error_msg

                    data = f.read()
                    actual_crc = binascii.crc32(data) & 0xFFFFFFFF
                    actual_size = len(data)

                    if p_id in part_map:
                        expected_crc_str = part_map[p_id].get("image", {}).get("crc32", "")
                        if expected_crc_str.startswith("0x"):
                            expected_crc = int(expected_crc_str, 16)
                            if actual_crc != expected_crc:
                                self.error_msg = f"{I18N.get('firmware', 'err_crc_mismatch', 'CRC mismatch for:')} {p_id}"
                                return False, self.error_msg

                    self.partitions.append({
                        "id": p_id,
                        "name_key": spec["name_key"],
                        "btn_label": spec["btn_label"],
                        "filename": fn,
                        "addr": spec["addr"],
                        "size": actual_size,
                        "crc32": f"0x{actual_crc:08X}",
                        "max_size": spec["max_size"],
                    })

            self.is_valid = True
            return True, I18N.get("firmware", "valid", "Valid Firmware Archive")

        except Exception as e:
            self.error_msg = str(e)
            return False, self.error_msg

    def extract_partition(self, partition_id: str, out_dir: str) -> Optional[str]:
        try:
            with tarfile.open(str(self.tar_path), "r") as tar:
                for p in self.partitions:
                    if p["id"] == partition_id:
                        fn = p["filename"]
                        tar.extract(fn, path=out_dir)
                        return os.path.join(out_dir, fn)
        except Exception:
            pass
        return None


# ==============================================================================
# Flashing Engine (OpenOCD Orchestrator)
# ==============================================================================
class FlashingEngine:
    def __init__(self,
                 on_log: Optional[Callable[[str], None]] = None,
                 on_part_progress: Optional[Callable[[str, float, str], None]] = None,
                 on_finish: Optional[Callable[[bool, str, str], None]] = None):
        self.on_log = on_log or (lambda msg: None)
        self.on_part_progress = on_part_progress or (lambda p, pct, s: None)
        self.on_finish = on_finish or (lambda ok, p, msg: None)
        self._is_flashing = False
        self._cancel_requested = False

    def log(self, msg: str) -> None:
        self.on_log(msg)

    def is_busy(self) -> bool:
        return self._is_flashing

    def cancel(self) -> None:
        self._cancel_requested = True

    def reset_device(self) -> Tuple[bool, str]:
        openocd = DeviceProbe.find_openocd()
        if not openocd:
            return False, I18N.get("log", "openocd_not_found", "OpenOCD executable not found!")

        cmd = [openocd]
        for sp in DeviceProbe.get_openocd_search_paths():
            cmd.extend(["-s", sp])

        cmd.extend([
            "-f", "interface/cmsis-dap.cfg",
            "-c", "cmsis_dap_vid_pid 0x28e9 0x0497 0x28e9 0x058f 0x28e9 0x018a",
            "-c", "transport select swd",
            "-f", "gd32h7x.cfg",
            "-c", "init; reset run; shutdown",
        ])

        try:
            self.log(I18N.get("actions", "resetting", "Resetting target MCU..."))
            res = subprocess.run(cmd, capture_output=True, text=True, timeout=8)
            if res.returncode == 0:
                self.log(I18N.get("actions", "reset_success", "Target reset successfully."))
                return True, I18N.get("actions", "reset_success", "Target reset successfully.")
            else:
                return False, "Reset command failed."
        except Exception as e:
            return False, str(e)

    def flash_partitions_async(self, pkg: FirmwarePackage, selected_ids: List[str]) -> threading.Thread:
        t = threading.Thread(target=self._run_flash, args=(pkg, selected_ids), daemon=True)
        t.start()
        return t

    def _run_flash(self, pkg: FirmwarePackage, selected_ids: List[str]) -> None:
        self._is_flashing = True
        self._cancel_requested = False

        openocd = DeviceProbe.find_openocd()
        if not openocd:
            err = I18N.get("log", "openocd_not_found", "OpenOCD executable not found!")
            self.log(f"<OSM> {err}")
            self._is_flashing = False
            self.on_finish(False, "", err)
            return

        with tempfile.TemporaryDirectory(prefix="aerodens_flash_") as temp_dir:
            parts_to_flash = [p for p in pkg.partitions if p["id"] in selected_ids]
            if not parts_to_flash:
                self.log("<OSM> No partitions selected for flashing.")
                self._is_flashing = False
                self.on_finish(False, "", "No partitions selected")
                return

            self.log("<OSM> Enter CS for TAR..")
            self.log(f"<OSM> Initializing flash process for {len(parts_to_flash)} partition(s)...")

            search_args = []
            for sp in DeviceProbe.get_openocd_search_paths():
                search_args.extend(["-s", sp])

            for idx, part in enumerate(parts_to_flash, 1):
                if self._cancel_requested:
                    self.log("<OSM> Flashing cancelled by user.")
                    self._is_flashing = False
                    self.on_finish(False, part["id"], "Cancelled")
                    return

                p_id = part["id"]
                p_addr = part["addr"]
                p_size = part["size"]
                self.log(f"<ID:0/001> Extracting {p_id} ({p_size} bytes)...")
                bin_file = pkg.extract_partition(p_id, temp_dir)
                if not bin_file or not os.path.isfile(bin_file):
                    err = f"Failed to extract {p_id} binary"
                    self.log(f"<ID:0/001> ERROR: {err}")
                    self._is_flashing = False
                    self.on_finish(False, p_id, err)
                    return

                self.on_part_progress(p_id, 0.05, I18N.get("partitions", "erasing", "ERASING..."))
                self.log(f"<ID:0/001> SetupConnection @ 0x{p_addr:08X}..")

                cmd = [
                    openocd,
                    *search_args,
                    "-f", "interface/cmsis-dap.cfg",
                    "-c", "cmsis_dap_vid_pid 0x28e9 0x0497 0x28e9 0x058f 0x28e9 0x018a",
                    "-c", "transport select swd",
                    "-f", "gd32h7x.cfg",
                    "-c", f"gd32h7_flash_bin {{{bin_file}}} 0x{p_addr:08X}",
                ]

                try:
                    proc = subprocess.Popen(
                        cmd,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        text=True,
                        bufsize=1,
                        universal_newlines=True,
                    )

                    for line in iter(proc.stdout.readline, ""):
                        line = line.strip()
                        if not line:
                            continue

                        if "Erasing" in line:
                            self.log(f"<ID:0/001> Erasing Flash.. ({p_id})")
                            self.on_part_progress(p_id, 0.15, I18N.get("partitions", "erasing", "ERASING..."))
                        elif "Programming chunk at offset" in line:
                            match = re.search(r"offset 0x([0-9A-Fa-f]+)\s+\((\d+)\s+bytes\)", line)
                            if match and p_size > 0:
                                cur_offset = int(match.group(1), 16)
                                cur_chunk = int(match.group(2))
                                frac = min(0.90, 0.15 + 0.75 * ((cur_offset + cur_chunk) / float(p_size)))
                                pct_text = f"{int(frac * 100)}%"
                                self.on_part_progress(p_id, frac, f"{I18N.get('partitions', 'programming', 'FLASHING...')} ({pct_text})")
                        elif "Verifying Flash" in line:
                            self.log(f"<ID:0/001> Verifying Flash.. ({p_id})")
                            self.on_part_progress(p_id, 0.95, I18N.get("partitions", "verifying", "VERIFYING..."))

                    proc.stdout.close()
                    ret = proc.wait()

                    if ret != 0:
                        err = f"OpenOCD error (code {ret}) during {p_id}"
                        self.log(f"<ID:0/001> FAIL! {err}")
                        self.on_part_progress(p_id, 0.0, I18N.get("partitions", "fail", "FAILED"))
                        self._is_flashing = False
                        self.on_finish(False, p_id, err)
                        return

                    self.on_part_progress(p_id, 1.0, I18N.get("partitions", "pass", "PASS (100%)"))
                    self.log(f"<ID:0/001> {p_id} .. RES OK !!")

                except Exception as e:
                    self.log(f"<ID:0/001> Exception: {e}")
                    self.on_part_progress(p_id, 0.0, I18N.get("partitions", "fail", "FAILED"))
                    self._is_flashing = False
                    self.on_finish(False, p_id, str(e))
                    return

            self.log("<ID:0/001> Resetting target MCU..")
            self.reset_device()
            self.log(f"<OSM> All threads completed. (succeed {len(parts_to_flash)} / failed 0)")
            self.log("<ID:0/001> Removed!!")

            self._is_flashing = False
            self.on_finish(True, "", "")


# ==============================================================================
# GUI Backend: GTK3 (Authentic Odin Light Mode for Linux)
# ==============================================================================
def run_gtk_gui():
    import gi
    gi.require_version("Gtk", "3.0")
    from gi.repository import Gtk, Gdk, GLib, GdkPixbuf

    # Authentic Odin classic light mode CSS
    css_provider = Gtk.CssProvider()
    css_data = b"""
    window {
        background-color: #EDEDED;
        color: #000000;
        font-family: Tahoma, 'Segoe UI', sans-serif;
        font-size: 11px;
    }
    .odin-banner {
        background-color: #FFFFFF;
        border-bottom: 1px solid #D5D5D5;
        padding: 6px 12px;
    }
    .odin-banner-card {
        background-color: #FFFFFF;
        border: none;
        padding: 2px 4px;
    }
    .odin-slot-box {
        background-color: #E0E0E0;
        border: 1px solid #B0B0B0;
        border-radius: 2px;
    }
    .odin-pass-ready {
        background-color: #E2E2E2;
        color: #666666;
        font-weight: bold;
        font-size: 14px;
        border: 1px solid #B5B5B5;
    }
    .odin-pass-active {
        background-color: #00C0EF;
        color: #FFFFFF;
        font-weight: bold;
        font-size: 14px;
        border: 1px solid #009AC0;
    }
    .odin-pass-success {
        background-color: #66CC00;
        color: #000000;
        font-weight: bold;
        font-size: 22px;
        border: 1px solid #4D9900;
    }
    .odin-pass-fail {
        background-color: #EE2200;
        color: #FFFFFF;
        font-weight: bold;
        font-size: 18px;
        border: 1px solid #CC0000;
    }
    .odin-com-bar {
        background-color: #EFEFEF;
        border: 1px solid #C0C0C0;
        font-family: monospace;
        font-size: 10px;
        font-weight: bold;
    }
    .odin-com-active {
        background-color: #74C0FC;
        color: #000000;
    }
    .odin-frame {
        border: 1px solid #C5C5C5;
        background-color: #F8F8F8;
        padding: 6px;
    }
    .odin-msg-box {
        background-color: #FFFFFF;
        color: #000000;
        font-family: 'Courier New', Courier, monospace;
        font-size: 11px;
        border: 1px solid #7F9DB9;
    }
    .odin-btn {
        background-color: #E1E1E1;
        border: 1px solid #707070;
        border-radius: 2px;
        color: #000000;
        padding: 3px 12px;
        font-weight: normal;
    }
    .odin-btn:hover {
        background-color: #E5F1FB;
        border-color: #0078D7;
    }
    .odin-btn-start {
        background-color: #E1E1E1;
        border: 1px solid #707070;
        font-weight: bold;
        padding: 6px 22px;
    }
    .odin-file-entry {
        background-color: #FFFFFF;
        color: #000000;
        font-family: 'Segoe UI', Tahoma, sans-serif;
        font-size: 11px;
        border: 1px solid #7F9DB9;
    }
    progressbar trough {
        background-color: #E6E6E6;
        border: 1px solid #B0B0B0;
        border-radius: 2px;
    }
    progressbar progress {
        background-color: #66CC00;
        border-radius: 1px;
    }
    """
    css_provider.load_from_data(css_data)
    screen = Gdk.Screen.get_default()
    if screen:
        Gtk.StyleContext.add_provider_for_screen(screen, css_provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)

    class GtkOdinFlasher(Gtk.Window):
        def __init__(self):
            super().__init__(title="AeroDENS Flasher (Odin3 GD32H759I)")
            self.set_default_size(840, 640)
            self.set_position(Gtk.WindowPosition.CENTER)
            self.connect("destroy", Gtk.main_quit)

            self.engine = FlashingEngine(
                on_log=self._on_engine_log,
                on_part_progress=self._on_engine_progress,
                on_finish=self._on_engine_finish,
            )
            self.pkg: Optional[FirmwarePackage] = None
            self.selected_device: Optional[Dict] = None
            self.part_widgets: Dict[str, Dict] = {}

            self._init_ui()
            GLib.timeout_add(300, self._auto_detect_startup)

        def _init_ui(self):
            main_vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
            self.add(main_vbox)

            # 1. Top Odin Banner
            banner_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
            banner_box.get_style_context().add_class("odin-banner")

            logo_card = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
            logo_card.get_style_context().add_class("odin-banner-card")

            if LOGO_SVG.is_file():
                try:
                    pb = GdkPixbuf.Pixbuf.new_from_file_at_scale(str(LOGO_SVG), 130, -1, True)
                    logo_img = Gtk.Image.new_from_pixbuf(pb)
                    logo_card.pack_start(logo_img, False, False, 0)
                except Exception:
                    pass

            lbl_banner_title = Gtk.Label()
            lbl_banner_title.set_markup("<span font_desc='Arial 9' color='#666666'>GD32H759I</span>")
            lbl_banner_title.set_valign(Gtk.Align.CENTER)
            logo_card.pack_start(lbl_banner_title, False, False, 0)
            banner_box.pack_start(logo_card, False, False, 0)

            # Language Selector on right (Clean: English / 简体中文)
            lang_hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=6)
            self.lbl_lang = Gtk.Label(label=I18N.get("app", "language", "Language") + ":")
            lang_hbox.pack_start(self.lbl_lang, False, False, 0)

            self.combo_lang = Gtk.ComboBoxText()
            self.combo_lang.append("en-us", "English")
            self.combo_lang.append("zh-cn", "简体中文")
            self.combo_lang.set_active(0 if I18N.lang_code == "en-us" else 1)
            self.combo_lang.connect("changed", self._on_lang_changed)
            lang_hbox.pack_start(self.combo_lang, False, False, 0)
            banner_box.pack_end(lang_hbox, False, False, 0)

            main_vbox.pack_start(banner_box, False, False, 0)

            content_vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
            content_vbox.set_margin_start(10)
            content_vbox.set_margin_end(10)
            content_vbox.set_margin_bottom(8)
            main_vbox.pack_start(content_vbox, True, True, 0)

            # 2. Odin ID:COM Slots Bar & File Select Row
            idcom_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)

            # Slot 1: The Iconic Odin PASS / Status Box
            slot1_vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
            slot1_vbox.set_size_request(120, 70)
            slot1_vbox.get_style_context().add_class("odin-slot-box")

            self.lbl_pass_box = Gtk.Label(label="IDLE")
            self.lbl_pass_box.set_size_request(118, 38)
            self.lbl_pass_box.get_style_context().add_class("odin-pass-ready")
            slot1_vbox.pack_start(self.lbl_pass_box, False, False, 0)

            self.pb_slot1 = Gtk.ProgressBar()
            self.pb_slot1.set_fraction(0.0)
            self.pb_slot1.set_size_request(118, 8)
            slot1_vbox.pack_start(self.pb_slot1, False, False, 0)

            self.lbl_com_port = Gtk.Label(label="ID:COM")
            self.lbl_com_port.set_size_request(118, 18)
            self.lbl_com_port.get_style_context().add_class("odin-com-bar")
            slot1_vbox.pack_start(self.lbl_com_port, False, False, 0)
            idcom_row.pack_start(slot1_vbox, False, False, 0)

            # Action Buttons & File Input
            actions_vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
            actions_btns = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)

            self.btn_connect = Gtk.Button(label=I18N.get("connection", "connect", "CONNECT"))
            self.btn_connect.get_style_context().add_class("odin-btn")
            self.btn_connect.connect("clicked", self._on_connect_clicked)
            actions_btns.pack_start(self.btn_connect, False, False, 0)

            self.btn_select_file = Gtk.Button(label=I18N.get("firmware", "select_file", "SELECT FILE"))
            self.btn_select_file.get_style_context().add_class("odin-btn")
            self.btn_select_file.connect("clicked", self._on_select_file_clicked)
            actions_btns.pack_start(self.btn_select_file, False, False, 0)

            self.lbl_device_info = Gtk.Label(label="Probe: Disconnected")
            self.lbl_device_info.set_halign(Gtk.Align.START)
            actions_btns.pack_start(self.lbl_device_info, False, False, 6)

            actions_vbox.pack_start(actions_btns, False, False, 0)

            # Tar file entry box
            self.entry_file = Gtk.Entry()
            self.entry_file.set_placeholder_text("Select .tar firmware package (e.g. aerodens_firmware.tar)")
            self.entry_file.set_editable(False)
            self.entry_file.get_style_context().add_class("odin-file-entry")
            actions_vbox.pack_start(self.entry_file, False, False, 0)

            idcom_row.pack_start(actions_vbox, True, True, 0)
            content_vbox.pack_start(idcom_row, False, False, 0)

            # 3. Main Split Body (Left: Option & Message | Right: Files [Download] LIST of partitions)
            split_hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)

            # LEFT PANE: Option & Message
            left_vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
            left_vbox.set_size_request(270, -1)

            opt_frame = Gtk.Frame(label=" Option ")
            opt_frame.get_style_context().add_class("odin-frame")
            opt_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=4)
            opt_box.set_margin_start(8)
            opt_box.set_margin_top(4)
            opt_box.set_margin_bottom(4)

            self.cb_auto_reboot = Gtk.CheckButton(label="Auto Reboot")
            self.cb_auto_reboot.set_active(True)
            opt_box.pack_start(self.cb_auto_reboot, False, False, 0)

            self.cb_f_reset = Gtk.CheckButton(label="F. Reset Time")
            self.cb_f_reset.set_active(True)
            opt_box.pack_start(self.cb_f_reset, False, False, 0)

            opt_frame.add(opt_box)
            left_vbox.pack_start(opt_frame, False, False, 0)

            # Message Live Console
            msg_frame = Gtk.Frame(label=" Message ")
            msg_frame.get_style_context().add_class("odin-frame")

            scroller = Gtk.ScrolledWindow()
            scroller.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)
            scroller.set_size_request(-1, 250)

            self.text_log = Gtk.TextView()
            self.text_log.set_editable(False)
            self.text_log.set_cursor_visible(False)
            self.text_log.get_style_context().add_class("odin-msg-box")
            self.log_buffer = self.text_log.get_buffer()
            scroller.add(self.text_log)
            msg_frame.add(scroller)
            left_vbox.pack_start(msg_frame, True, True, 0)

            split_hbox.pack_start(left_vbox, False, False, 0)

            # RIGHT PANE: Files [Download] - Authentic ODIN List Layout
            files_frame = Gtk.Frame(label=" Files [Download] ")
            files_frame.get_style_context().add_class("odin-frame")

            parts_list_vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
            parts_list_vbox.set_margin_start(10)
            parts_list_vbox.set_margin_end(10)
            parts_list_vbox.set_margin_top(10)
            parts_list_vbox.set_margin_bottom(10)

            for spec in DEFAULT_PARTITIONS:
                p_id = spec["id"]
                row_hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)

                # 1. Checkbox
                cb = Gtk.CheckButton()
                cb.set_active(True)
                row_hbox.pack_start(cb, False, False, 0)

                # 2. Odin Button (e.g. [Bootloader], [OS (APP)], [LOGO], [ANIMATION])
                btn_part = Gtk.Button(label=spec["btn_label"])
                btn_part.set_size_request(100, 28)
                btn_part.get_style_context().add_class("odin-btn")
                row_hbox.pack_start(btn_part, False, False, 0)

                # 3. Path / info entry box
                entry_info = Gtk.Entry()
                entry_info.set_text(f"0x{spec['addr']:08X} | {spec['filename']}")
                entry_info.set_editable(False)
                entry_info.get_style_context().add_class("odin-file-entry")
                row_hbox.pack_start(entry_info, True, True, 0)

                # 4. Individual Progress Bar (0% initial!)
                pb = Gtk.ProgressBar()
                pb.set_fraction(0.0)
                pb.set_size_request(100, 20)
                row_hbox.pack_start(pb, False, False, 0)

                # 5. Percent label
                lbl_pct = Gtk.Label(label="0%")
                lbl_pct.set_size_request(35, -1)
                row_hbox.pack_start(lbl_pct, False, False, 0)

                # 6. Status label
                lbl_status = Gtk.Label(label=I18N.get("partitions", "waiting", "WAITING"))
                lbl_status.set_size_request(65, -1)
                row_hbox.pack_start(lbl_status, False, False, 0)

                parts_list_vbox.pack_start(row_hbox, False, False, 0)

                self.part_widgets[p_id] = {
                    "cb": cb,
                    "btn": btn_part,
                    "entry": entry_info,
                    "pb": pb,
                    "lbl_pct": lbl_pct,
                    "lbl_status": lbl_status,
                    "name_key": spec["name_key"],
                    "filename": spec["filename"],
                    "addr": spec["addr"],
                }

            files_frame.add(parts_list_vbox)
            split_hbox.pack_start(files_frame, True, True, 0)

            content_vbox.pack_start(split_hbox, True, True, 0)

            # 4. Bottom Standard Button Row (Start, Reset, Exit)
            bottom_hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)

            btn_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
            self.btn_start = Gtk.Button(label=I18N.get("actions", "start", "Start"))
            self.btn_start.get_style_context().add_class("odin-btn")
            self.btn_start.get_style_context().add_class("odin-btn-start")
            self.btn_start.set_sensitive(False)
            self.btn_start.connect("clicked", self._on_start_clicked)
            btn_box.pack_start(self.btn_start, False, False, 0)

            self.btn_reset = Gtk.Button(label=I18N.get("actions", "reset", "Reset"))
            self.btn_reset.get_style_context().add_class("odin-btn")
            self.btn_reset.connect("clicked", self._on_reset_clicked)
            btn_box.pack_start(self.btn_reset, False, False, 0)

            btn_exit = Gtk.Button(label=I18N.get("actions", "exit", "Exit"))
            btn_exit.get_style_context().add_class("odin-btn")
            btn_exit.connect("clicked", lambda b: Gtk.main_quit())
            btn_box.pack_start(btn_exit, False, False, 0)

            bottom_hbox.pack_end(btn_box, False, False, 0)
            content_vbox.pack_start(bottom_hbox, False, False, 0)

        def _auto_detect_startup(self):
            devices = DeviceProbe.detect_devices()
            if devices:
                self.selected_device = devices[0]
                self._update_connection_status(True, self.selected_device["name"])
                self.engine.log(f"<ID:0/001> Added!! ({self.selected_device['name']})")
            else:
                self.engine.log("<OSM> Enter CS for MD5..")
                self.engine.log("<OSM> Check MD5.. Do not unplug the cable..")
                self.engine.log("<OSM> Please connect GD32 device via USB.")
            return False

        def _on_lang_changed(self, combo):
            code = combo.get_active_id()
            if code and code != I18N.lang_code:
                I18N.load(code)
                self._retranslate_ui()

        def _retranslate_ui(self):
            self.lbl_lang.set_text(I18N.get("app", "language", "Language") + ":")
            self.btn_connect.set_label(I18N.get("connection", "connect", "CONNECT"))
            self.btn_select_file.set_label(I18N.get("firmware", "select_file", "SELECT FILE"))
            self.btn_start.set_label(I18N.get("actions", "start", "Start"))
            self.btn_reset.set_label(I18N.get("actions", "reset", "Reset"))

        def _on_connect_clicked(self, widget):
            dialog = Gtk.Dialog(title=I18N.get("connection", "device_dialog_title", "Select GD32 Device"),
                                parent=self, flags=Gtk.DialogFlags.MODAL)
            dialog.set_default_size(500, 260)
            box = dialog.get_content_area()
            box.set_spacing(8)
            box.set_margin_top(10)
            box.set_margin_bottom(10)
            box.set_margin_start(12)
            box.set_margin_end(12)

            lbl = Gtk.Label(label=I18N.get("connection", "device_dialog_msg", "Detected GD32 Debug Probes:"))
            lbl.set_halign(Gtk.Align.START)
            box.pack_start(lbl, False, False, 0)

            liststore = Gtk.ListStore(str, str, str)
            treeview = Gtk.TreeView(model=liststore)
            for i, title in enumerate(["Device", "VID:PID", "Serial Number"]):
                renderer = Gtk.CellRendererText()
                col = Gtk.TreeViewColumn(title, renderer, text=i)
                treeview.append_column(col)

            devices = DeviceProbe.detect_devices()
            for d in devices:
                liststore.append([d["name"], f"{d['vid']}:{d['pid']}", d["serial"]])

            scroller = Gtk.ScrolledWindow()
            scroller.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)
            scroller.add(treeview)
            box.pack_start(scroller, True, True, 0)

            dialog.add_button(I18N.get("connection", "btn_cancel", "Cancel"), Gtk.ResponseType.CANCEL)
            dialog.add_button(I18N.get("connection", "btn_test_connect", "Test & Connect"), Gtk.ResponseType.OK)

            if devices:
                treeview.get_selection().select_path(Gtk.TreePath.new_first())

            dialog.show_all()
            resp = dialog.run()
            if resp == Gtk.ResponseType.OK:
                sel = treeview.get_selection()
                model, treeiter = sel.get_selected()
                if treeiter:
                    idx = model.get_path(treeiter).get_indices()[0]
                    target_dev = devices[idx] if idx < len(devices) else None
                    self._test_and_set_device(target_dev)
            dialog.destroy()

        def _test_and_set_device(self, dev: Optional[Dict]):
            self.engine.log(f"<OSM> {I18N.get('connection', 'status_testing', 'Testing connection to device...')}")
            ok, msg = DeviceProbe.test_connection(dev)
            self.engine.log(f"<ID:0/001> {msg}")
            if ok:
                self.selected_device = dev
                self._update_connection_status(True, dev["name"] if dev else "GD-Link")
                self._check_ready_to_flash()
            else:
                self._update_connection_status(False)
                self._show_msg_dialog(Gtk.MessageType.ERROR, I18N.get("connection", "status_fail", "Connection failed"), msg)

        def _update_connection_status(self, connected: bool, name: str = ""):
            ctx_pass = self.lbl_pass_box.get_style_context()
            ctx_com = self.lbl_com_port.get_style_context()

            ctx_pass.remove_class("odin-pass-ready")
            ctx_pass.remove_class("odin-pass-active")
            ctx_pass.remove_class("odin-pass-success")
            ctx_pass.remove_class("odin-pass-fail")
            ctx_com.remove_class("odin-com-active")

            if connected:
                ctx_pass.add_class("odin-pass-active")
                self.lbl_pass_box.set_text("READY")
                ctx_com.add_class("odin-com-active")
                self.lbl_com_port.set_text("0:[GD-Link]")
                self.lbl_device_info.set_text(f"Probe: {name}")
            else:
                ctx_pass.add_class("odin-pass-ready")
                self.lbl_pass_box.set_text("IDLE")
                self.lbl_com_port.set_text("ID:COM")
                self.lbl_device_info.set_text("Probe: Disconnected")

        def _on_select_file_clicked(self, widget):
            dialog = Gtk.FileChooserDialog(
                title=I18N.get("firmware", "select_file", "SELECT FILE"),
                parent=self,
                action=Gtk.FileChooserAction.OPEN,
            )
            dialog.add_button(I18N.get("connection", "btn_cancel", "Cancel"), Gtk.ResponseType.CANCEL)
            dialog.add_button(I18N.get("connection", "btn_test_connect", "Open"), Gtk.ResponseType.OK)

            filter_tar = Gtk.FileFilter()
            filter_tar.set_name(I18N.get("firmware", "tar_filter_name", "Firmware Archive (*.tar)"))
            filter_tar.add_pattern("*.tar")
            dialog.add_filter(filter_tar)

            default_build = PROJECT_ROOT / "build"
            if default_build.is_dir():
                dialog.set_current_folder(str(default_build))

            resp = dialog.run()
            if resp == Gtk.ResponseType.OK:
                path = dialog.get_filename()
                dialog.destroy()
                self._load_and_validate_tar(path)
            else:
                dialog.destroy()

        def _load_and_validate_tar(self, tar_path: str):
            self.engine.log(f"<OSM> Check TAR: {tar_path}..")
            pkg = FirmwarePackage(tar_path)
            ok, msg = pkg.validate()
            if ok:
                self.pkg = pkg
                sz_mb = round(pkg.tar_path.stat().st_size / (1024 * 1024), 2)
                self.entry_file.set_text(f"{pkg.tar_path} ({sz_mb} MB)")
                self.engine.log(f"<OSM> File analysis.. OK ({len(pkg.partitions)} partitions verified)")

                for p in pkg.partitions:
                    p_id = p["id"]
                    if p_id in self.part_widgets:
                        w = self.part_widgets[p_id]
                        sz_kb = round(p["size"] / 1024.0, 1)
                        w["entry"].set_text(f"{p['filename']} | 0x{p['addr']:08X} | {sz_kb} KB | {p['crc32']}")
                        w["lbl_status"].set_text("READY")
                self._check_ready_to_flash()
            else:
                self.pkg = None
                self.entry_file.set_text("")
                self.engine.log(f"<OSM> Validation error: {msg}")
                self._show_msg_dialog(Gtk.MessageType.ERROR, I18N.get("firmware", "invalid", "Invalid Firmware"), msg)
                self.btn_start.set_sensitive(False)

        def _check_ready_to_flash(self):
            ready = bool(self.selected_device and self.pkg and self.pkg.is_valid and not self.engine.is_busy())
            self.btn_start.set_sensitive(ready)

        def _on_start_clicked(self, widget):
            if not self.pkg or not self.selected_device:
                return

            selected_ids = []
            for p_id, w in self.part_widgets.items():
                if w["cb"].get_active():
                    selected_ids.append(p_id)
                else:
                    w["lbl_status"].set_text(I18N.get("partitions", "skipped", "SKIPPED"))

            if not selected_ids:
                self._show_msg_dialog(Gtk.MessageType.WARNING, I18N.get("actions", "title", "Warning"),
                                       I18N.get("actions", "no_partitions_selected", "Select at least 1 partition!"))
                return

            for p_id in selected_ids:
                w = self.part_widgets[p_id]
                w["pb"].set_fraction(0.0)
                w["lbl_pct"].set_text("0%")
                w["lbl_status"].set_text(I18N.get("partitions", "waiting", "WAITING"))

            self.btn_start.set_sensitive(False)
            self.btn_reset.set_sensitive(False)
            self.btn_connect.set_sensitive(False)
            self.btn_select_file.set_sensitive(False)

            self.lbl_pass_box.get_style_context().remove_class("odin-pass-success")
            self.lbl_pass_box.get_style_context().remove_class("odin-pass-fail")
            self.lbl_pass_box.get_style_context().add_class("odin-pass-active")
            self.lbl_pass_box.set_text("FLASHING")

            self.engine.flash_partitions_async(self.pkg, selected_ids)

        def _on_reset_clicked(self, widget):
            threading.Thread(target=self.engine.reset_device, daemon=True).start()

        def _on_engine_log(self, text: str):
            GLib.idle_add(self._append_log, text)

        def _append_log(self, text: str):
            end_iter = self.log_buffer.get_end_iter()
            self.log_buffer.insert(end_iter, text + "\n")
            adj = self.text_log.get_parent().get_vadjustment()
            if adj:
                adj.set_value(adj.get_upper())
            return False

        def _on_engine_progress(self, part_id: str, frac: float, status_text: str):
            GLib.idle_add(self._update_progress_ui, part_id, frac, status_text)

        def _update_progress_ui(self, part_id: str, frac: float, status_text: str):
            if part_id in self.part_widgets:
                w = self.part_widgets[part_id]
                w["pb"].set_fraction(frac)
                pct_str = f"{int(frac * 100)}%"
                w["lbl_pct"].set_text(pct_str)
                w["lbl_status"].set_text(status_text)
                self.pb_slot1.set_fraction(frac)
            return False

        def _on_engine_finish(self, success: bool, part_id: str, err_msg: str):
            GLib.idle_add(self._finish_ui, success, part_id, err_msg)

        def _finish_ui(self, success: bool, part_id: str, err_msg: str):
            self.btn_reset.set_sensitive(True)
            self.btn_connect.set_sensitive(True)
            self.btn_select_file.set_sensitive(True)
            self._check_ready_to_flash()

            ctx = self.lbl_pass_box.get_style_context()
            ctx.remove_class("odin-pass-active")

            if success:
                ctx.add_class("odin-pass-success")
                self.lbl_pass_box.set_text("PASS!")
                self.pb_slot1.set_fraction(1.0)
                self._show_big_check_success_dialog()
            else:
                ctx.add_class("odin-pass-fail")
                self.lbl_pass_box.set_text("FAIL!")
                self._show_msg_dialog(
                    Gtk.MessageType.ERROR,
                    I18N.get("result", "fail_title", "Flash Failed"),
                    f"{I18N.get('result', 'fail_msg', 'Error in:')} {part_id}\n\n{err_msg}",
                )
            return False

        def _show_big_check_success_dialog(self):
            dialog = Gtk.Dialog(title=I18N.get("result", "success_title", "Flash Success"),
                                parent=self, flags=Gtk.DialogFlags.MODAL)
            dialog.set_default_size(360, 220)
            box = dialog.get_content_area()
            box.set_spacing(10)
            box.set_margin_top(16)
            box.set_margin_bottom(16)
            box.set_margin_start(16)
            box.set_margin_end(16)

            lbl_check = Gtk.Label()
            lbl_check.set_markup("<span font_desc='Arial 44' color='#4D9900'><b>&#x2714;</b></span>")
            box.pack_start(lbl_check, False, False, 0)

            lbl_title = Gtk.Label()
            lbl_title.set_markup(f"<span size='large' font_weight='bold'>{I18N.get('result', 'success_title', 'Flash Success')}</span>")
            box.pack_start(lbl_title, False, False, 0)

            lbl_msg = Gtk.Label(label=I18N.get("result", "success_msg", "All partitions flashed successfully!"))
            lbl_msg.set_line_wrap(True)
            box.pack_start(lbl_msg, False, False, 0)

            btn = dialog.add_button("OK", Gtk.ResponseType.OK)
            btn.get_style_context().add_class("odin-btn")

            dialog.show_all()
            dialog.run()
            dialog.destroy()

        def _show_msg_dialog(self, mtype, title, msg):
            dlg = Gtk.MessageDialog(parent=self, flags=Gtk.DialogFlags.MODAL, type=mtype,
                                    buttons=Gtk.ButtonsType.OK, message_format=title)
            dlg.format_secondary_text(msg)
            dlg.run()
            dlg.destroy()

    app = GtkOdinFlasher()
    app.show_all()
    Gtk.main()


# ==============================================================================
# GUI Backend: Tkinter (Authentic Odin Light Mode for Windows / Generic Linux)
# ==============================================================================
def run_tk_gui():
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk

    class TkOdinFlasher(tk.Tk):
        def __init__(self):
            super().__init__()
            self.title("AeroDENS Flasher (Odin3 GD32H759I)")
            self.geometry("860x650")
            self.configure(bg="#EDEDED")

            self.engine = FlashingEngine(
                on_log=self._on_engine_log,
                on_part_progress=self._on_engine_progress,
                on_finish=self._on_engine_finish,
            )
            self.pkg: Optional[FirmwarePackage] = None
            self.selected_device: Optional[Dict] = None
            self.part_widgets: Dict[str, Dict] = {}

            self._init_ui()
            self.after(300, self._auto_detect_startup)

        def _init_ui(self):
            banner_frame = tk.Frame(self, bg="#FFFFFF", padx=10, pady=6)
            banner_frame.pack(fill=tk.X, side=tk.TOP)

            card_logo = tk.Frame(banner_frame, bg="#FFFFFF")
            card_logo.pack(side=tk.LEFT)

            self.logo_img = None
            if LOGO_PNG.is_file():
                try:
                    self.logo_img = tk.PhotoImage(file=str(LOGO_PNG))
                    tk.Label(card_logo, image=self.logo_img, bg="#FFFFFF").pack(side=tk.LEFT, padx=4)
                except Exception:
                    pass

            tk.Label(card_logo, text="GD32H759I", font=("Arial", 9), fg="#666666", bg="#FFFFFF").pack(side=tk.LEFT, padx=4)

            # Language switcher (clean English / 简体中文)
            lang_frame = tk.Frame(banner_frame, bg="#FFFFFF")
            lang_frame.pack(side=tk.RIGHT, padx=6)
            self.lbl_lang = tk.Label(lang_frame, text=I18N.get("app", "language", "Language") + ":", bg="#FFFFFF", fg="#333333")
            self.lbl_lang.pack(side=tk.LEFT, padx=4)

            self.var_lang = tk.StringVar(value="English" if I18N.lang_code == "en-us" else "简体中文")
            cb_lang = ttk.Combobox(lang_frame, textvariable=self.var_lang, values=["English", "简体中文"], width=10, state="readonly")
            cb_lang.pack(side=tk.LEFT)
            cb_lang.bind("<<ComboboxSelected>>", self._on_lang_changed)

            sep = tk.Frame(self, bg="#D5D5D5", height=1)
            sep.pack(fill=tk.X, side=tk.TOP)

            content_frame = tk.Frame(self, bg="#EDEDED", padx=10, pady=8)
            content_frame.pack(fill=tk.BOTH, expand=True)

            # ID:COM Row & File Select
            idcom_row = tk.Frame(content_frame, bg="#EDEDED")
            idcom_row.pack(fill=tk.X, pady=4)

            # Slot 1: PASS / IDLE Box
            slot1 = tk.Frame(idcom_row, bg="#E0E0E0", bd=1, relief=tk.SOLID, width=120, height=70)
            slot1.pack(side=tk.LEFT, padx=4)
            slot1.pack_propagate(False)

            self.lbl_pass_box = tk.Label(slot1, text="IDLE", font=("Arial", 11, "bold"), bg="#E2E2E2", fg="#555555")
            self.lbl_pass_box.pack(fill=tk.X, ipady=4)

            self.pb_slot1 = ttk.Progressbar(slot1, orient="horizontal", mode="determinate")
            self.pb_slot1.pack(fill=tk.X, padx=2, pady=2)
            self.pb_slot1["value"] = 0

            self.lbl_com_port = tk.Label(slot1, text="ID:COM", font=("Consolas", 8, "bold"), bg="#EFEFEF", fg="#000000")
            self.lbl_com_port.pack(fill=tk.X)

            # Controls next to slot 1
            ctrls = tk.Frame(idcom_row, bg="#EDEDED")
            ctrls.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=6)

            btns_line = tk.Frame(ctrls, bg="#EDEDED")
            btns_line.pack(fill=tk.X)

            self.btn_connect = tk.Button(btns_line, text=I18N.get("connection", "connect", "CONNECT"),
                                         command=self._on_connect_clicked, padx=12, pady=2)
            self.btn_connect.pack(side=tk.LEFT, padx=4)

            self.btn_select_file = tk.Button(btns_line, text=I18N.get("firmware", "select_file", "SELECT FILE"),
                                             command=self._on_select_file_clicked, padx=12, pady=2)
            self.btn_select_file.pack(side=tk.LEFT, padx=4)

            self.lbl_dev_info = tk.Label(btns_line, text="Probe: Disconnected", bg="#EDEDED", fg="#333333", font=("Segoe UI", 8))
            self.lbl_dev_info.pack(side=tk.LEFT, padx=6)

            self.entry_file = tk.Entry(ctrls, bg="#FFFFFF", fg="#000000")
            self.entry_file.pack(fill=tk.X, padx=4, pady=4)

            # Split Main Body
            split_row = tk.Frame(content_frame, bg="#EDEDED")
            split_row.pack(fill=tk.BOTH, expand=True, pady=6)

            # Left Pane: Option & Message
            left_pane = tk.Frame(split_row, bg="#EDEDED", width=270)
            left_pane.pack(side=tk.LEFT, fill=tk.BOTH, padx=4)

            opt_frame = tk.LabelFrame(left_pane, text=" Option ", bg="#EDEDED", padx=6, pady=4)
            opt_frame.pack(fill=tk.X)
            tk.Checkbutton(opt_frame, text="Auto Reboot", bg="#EDEDED").pack(anchor="w")
            tk.Checkbutton(opt_frame, text="F. Reset Time", bg="#EDEDED").pack(anchor="w")

            msg_frame = tk.LabelFrame(left_pane, text=" Message ", bg="#EDEDED", padx=4, pady=4)
            msg_frame.pack(fill=tk.BOTH, expand=True, pady=4)

            self.txt_log = tk.Text(msg_frame, bg="#FFFFFF", fg="#000000", font=("Courier", 8), height=14)
            self.txt_log.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
            scroller = tk.Scrollbar(msg_frame, command=self.txt_log.yview)
            scroller.pack(side=tk.RIGHT, fill=tk.Y)
            self.txt_log.config(yscrollcommand=scroller.set)

            # Right Pane: Files [Download] - Authentic ODIN List Layout
            right_pane = tk.LabelFrame(split_row, text=" Files [Download] ", bg="#EDEDED", padx=10, pady=10)
            right_pane.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=4)

            for spec in DEFAULT_PARTITIONS:
                p_id = spec["id"]
                row_f = tk.Frame(right_pane, bg="#EDEDED", pady=4)
                row_f.pack(fill=tk.X)

                var_cb = tk.BooleanVar(value=True)
                cb = tk.Checkbutton(row_f, variable=var_cb, bg="#EDEDED")
                cb.pack(side=tk.LEFT, padx=2)

                btn_p = tk.Button(row_f, text=spec["btn_label"], width=13, relief=tk.RAISED, pady=1)
                btn_p.pack(side=tk.LEFT, padx=4)

                entry_info = tk.Entry(row_f, bg="#FFFFFF", fg="#000000")
                entry_info.insert(0, f"0x{spec['addr']:08X} | {spec['filename']}")
                entry_info.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=4)

                pb = ttk.Progressbar(row_f, orient="horizontal", length=90, mode="determinate")
                pb.pack(side=tk.LEFT, padx=4)
                pb["value"] = 0

                lbl_pct = tk.Label(row_f, text="0%", width=4, bg="#EDEDED", font=("Segoe UI", 8))
                lbl_pct.pack(side=tk.LEFT)

                lbl_status = tk.Label(row_f, text=I18N.get("partitions", "waiting", "WAITING"), width=8, bg="#EDEDED", font=("Segoe UI", 8))
                lbl_status.pack(side=tk.LEFT, padx=2)

                self.part_widgets[p_id] = {
                    "var_cb": var_cb,
                    "cb": cb,
                    "btn": btn_p,
                    "entry": entry_info,
                    "pb": pb,
                    "lbl_pct": lbl_pct,
                    "lbl_status": lbl_status,
                    "name_key": spec["name_key"],
                    "filename": spec["filename"],
                    "addr": spec["addr"],
                }

            # Bottom Bar (Start, Reset, Exit)
            bottom_bar = tk.Frame(content_frame, bg="#EDEDED", pady=6)
            bottom_bar.pack(fill=tk.X, side=tk.BOTTOM)

            btns_r = tk.Frame(bottom_bar, bg="#EDEDED")
            btns_r.pack(side=tk.RIGHT)

            self.btn_start = tk.Button(btns_r, text=I18N.get("actions", "start", "Start"),
                                       state=tk.DISABLED, command=self._on_start_clicked, padx=18, pady=4, font=("Segoe UI", 9, "bold"))
            self.btn_start.pack(side=tk.LEFT, padx=4)

            self.btn_reset = tk.Button(btns_r, text=I18N.get("actions", "reset", "Reset"),
                                       command=self._on_reset_clicked, padx=14, pady=4)
            self.btn_reset.pack(side=tk.LEFT, padx=4)

            tk.Button(btns_r, text=I18N.get("actions", "exit", "Exit"), command=self.destroy, padx=14, pady=4).pack(side=tk.LEFT, padx=4)

        def _auto_detect_startup(self):
            devices = DeviceProbe.detect_devices()
            if devices:
                self.selected_device = devices[0]
                self._update_conn(True, self.selected_device["name"])
                self.engine.log(f"<ID:0/001> Added!! ({self.selected_device['name']})")
            else:
                self.engine.log("<OSM> Enter CS for MD5..")
                self.engine.log("<OSM> Check MD5.. Do not unplug the cable..")
                self.engine.log("<OSM> Please connect GD32 device via USB.")

        def _on_lang_changed(self, event=None):
            val = self.var_lang.get()
            code = "zh-cn" if "中文" in val else "en-us"
            I18N.load(code)
            self.lbl_lang.config(text=I18N.get("app", "language", "Language") + ":")
            self.btn_connect.config(text=I18N.get("connection", "connect", "CONNECT"))
            self.btn_select_file.config(text=I18N.get("firmware", "select_file", "SELECT FILE"))
            self.btn_start.config(text=I18N.get("actions", "start", "Start"))
            self.btn_reset.config(text=I18N.get("actions", "reset", "Reset"))

        def _on_connect_clicked(self):
            dlg = tk.Toplevel(self)
            dlg.title(I18N.get("connection", "device_dialog_title", "Select GD32 Device"))
            dlg.geometry("480x260")
            dlg.configure(bg="#EDEDED")
            dlg.transient(self)
            dlg.grab_set()

            tk.Label(dlg, text=I18N.get("connection", "device_dialog_msg", "Detected GD32 Debug Probes:"),
                     bg="#EDEDED", font=("Segoe UI", 9, "bold")).pack(anchor="w", padx=10, pady=6)

            lb = tk.Listbox(dlg, bg="#FFFFFF", fg="#000000", font=("Consolas", 9))
            lb.pack(fill=tk.BOTH, expand=True, padx=10, pady=4)

            devices = DeviceProbe.detect_devices()
            for d in devices:
                lb.insert(tk.END, d["desc"])

            if devices:
                lb.select_set(0)

            def do_connect():
                sel = lb.curselection()
                if sel:
                    dev = devices[sel[0]]
                    dlg.destroy()
                    self.engine.log(f"<OSM> {I18N.get('connection', 'status_testing', 'Testing connection...')}")
                    ok, msg = DeviceProbe.test_connection(dev)
                    self.engine.log(f"<ID:0/001> {msg}")
                    if ok:
                        self.selected_device = dev
                        self._update_conn(True, dev["name"])
                        self._check_ready()
                    else:
                        self.selected_device = None
                        self._update_conn(False)
                        messagebox.showerror(I18N.get("connection", "status_fail", "Connection failed"), msg, parent=self)

            btn_f = tk.Frame(dlg, bg="#EDEDED", pady=6)
            btn_f.pack(fill=tk.X)
            tk.Button(btn_f, text=I18N.get("connection", "btn_test_connect", "Test & Connect"),
                      command=do_connect, padx=12, pady=2).pack(side=tk.RIGHT, padx=10)
            tk.Button(btn_f, text=I18N.get("connection", "btn_cancel", "Cancel"), command=dlg.destroy).pack(side=tk.RIGHT)

        def _update_conn(self, connected: bool, name: str = ""):
            if connected:
                self.lbl_pass_box.config(text="READY", bg="#00C0EF", fg="#FFFFFF")
                self.lbl_com_port.config(text="0:[GD-Link]", bg="#74C0FC")
                self.lbl_dev_info.config(text=f"Probe: {name}")
            else:
                self.lbl_pass_box.config(text="IDLE", bg="#E2E2E2", fg="#555555")
                self.lbl_com_port.config(text="ID:COM", bg="#EFEFEF")
                self.lbl_dev_info.config(text="Probe: Disconnected")

        def _on_select_file_clicked(self):
            default_dir = str(PROJECT_ROOT / "build") if (PROJECT_ROOT / "build").is_dir() else str(PROJECT_ROOT)
            f = filedialog.askopenfilename(
                title=I18N.get("firmware", "select_file", "SELECT FILE"),
                initialdir=default_dir,
                filetypes=[(I18N.get("firmware", "tar_filter_name", "Firmware Archive (*.tar)"), "*.tar")],
                parent=self,
            )
            if f:
                self.engine.log(f"<OSM> Check TAR: {f}..")
                pkg = FirmwarePackage(f)
                ok, msg = pkg.validate()
                if ok:
                    self.pkg = pkg
                    sz_mb = round(pkg.tar_path.stat().st_size / (1024 * 1024), 2)
                    self.entry_file.delete(0, tk.END)
                    self.entry_file.insert(0, f"{pkg.tar_path} ({sz_mb} MB)")
                    self.engine.log(f"<OSM> File analysis.. OK ({len(pkg.partitions)} partitions verified)")
                    for p in pkg.partitions:
                        p_id = p["id"]
                        if p_id in self.part_widgets:
                            w = self.part_widgets[p_id]
                            sz_kb = round(p["size"] / 1024.0, 1)
                            w["entry"].delete(0, tk.END)
                            w["entry"].insert(0, f"{p['filename']} | 0x{p['addr']:08X} | {sz_kb} KB | {p['crc32']}")
                            w["lbl_status"].config(text="READY")
                    self._check_ready()
                else:
                    self.pkg = None
                    self.entry_file.delete(0, tk.END)
                    messagebox.showerror(I18N.get("firmware", "invalid", "Invalid Firmware"), msg, parent=self)
                    self.btn_start.config(state=tk.DISABLED)

        def _check_ready(self):
            if self.pkg and self.pkg.is_valid and self.selected_device and not self.engine.is_busy():
                self.btn_start.config(state=tk.NORMAL)
            else:
                self.btn_start.config(state=tk.DISABLED)

        def _on_start_clicked(self):
            selected_ids = [p_id for p_id, w in self.part_widgets.items() if w["var_cb"].get()]
            if not selected_ids:
                messagebox.showwarning(I18N.get("actions", "title", "Warning"),
                                       I18N.get("actions", "no_partitions_selected", "Select at least 1 partition!"), parent=self)
                return

            for p_id in selected_ids:
                w = self.part_widgets[p_id]
                w["pb"]["value"] = 0
                w["lbl_pct"].config(text="0%")
                w["lbl_status"].config(text=I18N.get("partitions", "waiting", "WAITING"))

            self.btn_start.config(state=tk.DISABLED)
            self.btn_reset.config(state=tk.DISABLED)
            self.btn_connect.config(state=tk.DISABLED)
            self.btn_select_file.config(state=tk.DISABLED)

            self.lbl_pass_box.config(text="FLASHING", bg="#00C0EF", fg="#FFFFFF")
            self.engine.flash_partitions_async(self.pkg, selected_ids)

        def _on_reset_clicked(self):
            threading.Thread(target=self.engine.reset_device, daemon=True).start()

        def _on_engine_log(self, text: str):
            self.after(0, self._append_log, text)

        def _append_log(self, text: str):
            self.txt_log.insert(tk.END, text + "\n")
            self.txt_log.see(tk.END)

        def _on_engine_progress(self, part_id: str, frac: float, status_text: str):
            self.after(0, self._update_part_ui, part_id, frac, status_text)

        def _update_part_ui(self, part_id: str, frac: float, status_text: str):
            if part_id in self.part_widgets:
                w = self.part_widgets[part_id]
                w["pb"]["value"] = frac * 100
                w["lbl_pct"].config(text=f"{int(frac * 100)}%")
                w["lbl_status"].config(text=status_text)
                self.pb_slot1["value"] = frac * 100

        def _on_engine_finish(self, success: bool, part_id: str, err_msg: str):
            self.after(0, self._finish_ui, success, part_id, err_msg)

        def _finish_ui(self, success: bool, part_id: str, err_msg: str):
            self.btn_reset.config(state=tk.NORMAL)
            self.btn_connect.config(state=tk.NORMAL)
            self.btn_select_file.config(state=tk.NORMAL)
            self._check_ready()

            if success:
                self.lbl_pass_box.config(text="PASS!", bg="#66CC00", fg="#000000", font=("Arial", 16, "bold"))
                self.pb_slot1["value"] = 100
                self._show_big_check_success_dialog()
            else:
                self.lbl_pass_box.config(text="FAIL!", bg="#EE2200", fg="#FFFFFF")
                messagebox.showerror(
                    I18N.get("result", "fail_title", "Flash Failed"),
                    f"{I18N.get('result', 'fail_msg', 'Error in:')} {part_id}\n\n{err_msg}",
                    parent=self
                )

        def _show_big_check_success_dialog(self):
            dlg = tk.Toplevel(self)
            dlg.title(I18N.get("result", "success_title", "Flash Success"))
            dlg.geometry("360x220")
            dlg.configure(bg="#EDEDED")
            dlg.transient(self)
            dlg.grab_set()

            tk.Label(dlg, text="\u2714", font=("Arial", 42, "bold"), fg="#4D9900", bg="#EDEDED").pack(pady=6)
            tk.Label(dlg, text=I18N.get("result", "success_title", "Flash Success"),
                     font=("Segoe UI", 12, "bold"), fg="#000000", bg="#EDEDED").pack()
            tk.Label(dlg, text=I18N.get("result", "success_msg", "All partitions flashed successfully!"),
                     font=("Segoe UI", 9), fg="#333333", bg="#EDEDED", wraplength=300).pack(pady=4)

            tk.Button(dlg, text="OK", font=("Segoe UI", 9, "bold"), command=dlg.destroy, padx=20, pady=2).pack(pady=10)

    app = TkOdinFlasher()
    app.mainloop()


# ==============================================================================
# CLI Backend (Command-Line Mode)
# ==============================================================================
def run_cli_mode(args):
    print("================================================================================")
    print(" AeroDENS GD32H759 Firmware Flasher (CLI Mode)")
    print("================================================================================")

    if args.lang:
        I18N.load(args.lang)

    # 1. List devices
    if args.list_devices:
        devices = DeviceProbe.detect_devices()
        print(f"\nDetected {len(devices)} GD32 Device(s):")
        for i, d in enumerate(devices, 1):
            print(f"  [{i}] {d['name']} | VID={d['vid']} PID={d['pid']} S/N={d['serial']} ({d['bus']})")
        return 0

    # 2. Test Connection
    if args.connect:
        print("\nTesting connection to GD32 device...")
        ok, msg = DeviceProbe.test_connection()
        print(f"Result: {'PASS' if ok else 'FAIL'} - {msg}")
        if not args.file and not args.flash and not args.reset:
            return 0 if ok else 1

    # 3. Direct Target Reset
    if args.reset and not args.file and not args.flash:
        print("\nResetting GD32H759 target MCU...")
        engine = FlashingEngine(on_log=print)
        ok, msg = engine.reset_device()
        print(f"Result: {'PASS' if ok else 'FAIL'} - {msg}")
        return 0 if ok else 1

    # 4. Validate & Flash file
    if not args.file:
        print("\nError: No firmware TAR file specified. Use --file <path/to/firmware.tar>")
        return 1

    pkg = FirmwarePackage(args.file)
    print(f"\nAnalyzing firmware archive: {args.file}...")
    ok, msg = pkg.validate()
    if not ok:
        print(f"Error: {msg}")
        return 1

    print(f"Validation OK: Found {len(pkg.partitions)} partition(s):")
    for p in pkg.partitions:
        print(f"  - {p['id']:10s} : Addr=0x{p['addr']:08X} Size={p['size']:7d} B CRC32={p['crc32']}")

    if not args.flash and not args.reset:
        print("\nVerification successful. Add --flash to execute programming.")
        return 0

    if args.partitions:
        req_parts = [x.strip().lower() for x in args.partitions.split(",")]
        selected_ids = [p["id"] for p in pkg.partitions if p["id"] in req_parts]
    else:
        selected_ids = [p["id"] for p in pkg.partitions]

    print(f"\nSelected partitions to flash: {', '.join(selected_ids)}")

    flash_finished_event = threading.Event()
    flash_success = False

    def on_cli_log(line: str):
        print(line)

    def on_cli_progress(part: str, frac: float, status: str):
        pct = int(frac * 100)
        bar_len = 30
        filled = int(bar_len * frac)
        bar = "=" * filled + "-" * (bar_len - filled)
        sys.stdout.write(f"\r  [{part:10s}] [{bar}] {pct:3d}% - {status}")
        sys.stdout.flush()
        if frac >= 1.0:
            sys.stdout.write("\n")

    def on_cli_finish(ok: bool, part: str, err: str):
        nonlocal flash_success
        flash_success = ok
        flash_finished_event.set()

    engine = FlashingEngine(on_log=on_cli_log, on_part_progress=on_cli_progress, on_finish=on_cli_finish)

    print("\nStarting flash sequence...")
    engine.flash_partitions_async(pkg, selected_ids)
    flash_finished_event.wait()

    if flash_success:
        print("\n================================================================================")
        print(" [PASS] Flash Success! Target MCU has been reset and is running AeroDENS.")
        print("================================================================================")
        return 0
    else:
        print("\n================================================================================")
        print(" [FAIL] Flashing encountered an error!")
        print("================================================================================")
        return 1


# ==============================================================================
# Main Entry Point
# ==============================================================================
def main():
    parser = argparse.ArgumentParser(description="AeroDENS Odin-Style GD32H759 Firmware Flasher")
    parser.add_argument("--cli", action="store_true", help="Force command-line interface (no GUI)")
    parser.add_argument("-f", "--file", type=str, help="Path to .tar firmware package")
    parser.add_argument("-c", "--connect", action="store_true", help="Test connection to GD32 device")
    parser.add_argument("-l", "--list-devices", action="store_true", help="List detected GD32 devices")
    parser.add_argument("--flash", action="store_true", help="Execute flashing of selected partitions")
    parser.add_argument("--partitions", type=str, help="Comma-separated partitions to flash (e.g. bootloader,os,logo,anim)")
    parser.add_argument("--reset", action="store_true", help="Reset the GD32 target device")
    parser.add_argument("--lang", type=str, choices=["en-us", "zh-cn"], default="en-us", help="Language code")

    args = parser.parse_args()
    is_cli = args.cli or args.file or args.connect or args.list_devices or args.flash or args.reset

    if is_cli:
        sys.exit(run_cli_mode(args))

    has_display = bool(os.environ.get("WAYLAND_DISPLAY") or os.environ.get("DISPLAY"))
    if not has_display and sys.platform.startswith("linux"):
        print("Notice: No graphical display server detected. Running in CLI interactive mode.")
        sys.exit(run_cli_mode(args))

    # Try GTK3 (Native Linux)
    if sys.platform.startswith("linux"):
        try:
            import gi
            gi.require_version("Gtk", "3.0")
            from gi.repository import Gtk
            ok, _ = Gtk.init_check()
            if ok:
                run_gtk_gui()
                return
        except Exception:
            pass

    # Try Tkinter (Windows & Generic Linux)
    try:
        import tkinter
        run_tk_gui()
        return
    except Exception as e:
        print(f"Notice: GUI could not be initialized ({e}). Falling back to CLI mode.")
        sys.exit(run_cli_mode(args))


if __name__ == "__main__":
    main()
