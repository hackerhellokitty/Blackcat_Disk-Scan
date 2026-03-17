"""
BlackCat Disk Scanner v3.1
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Tool 22  — HDD / SSD sector-level read scan
Platform — Windows (Administrator required for raw access)
UI       — Flet >= 0.80
Languages — EN / TH / JA / ZH  (live switch, no restart)
Install  — pip install flet
Run      — python blackcat_disk_scanner.py
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
v4.0 roadmap: PDF report generation after scan
"""

# ── stdlib ─────────────────────────────────────────────────────────────────────
import ctypes
import ctypes.wintypes
import platform
import random
import subprocess
import threading
import time

# ── third-party ────────────────────────────────────────────────────────────────
import flet as ft


# ══════════════════════════════════════════════════════════════════════════════
#  ADMIN CHECK
# ══════════════════════════════════════════════════════════════════════════════

def is_admin() -> bool:
    """Return True if the current process has Administrator privileges."""
    if platform.system() != "Windows":
        return True   # non-Windows: no restriction, treat as OK
    try:
        return bool(ctypes.windll.shell32.IsUserAnAdmin())
    except Exception:
        return False


def relaunch_as_admin() -> None:
    """Re-launch this script elevated via ShellExecute runas verb."""
    import sys
    ctypes.windll.shell32.ShellExecuteW(
        None, "runas",
        sys.executable,
        f'"{sys.argv[0]}"',
        None, 1
    )


# ══════════════════════════════════════════════════════════════════════════════
#  CONSTANTS — theme & grid
# ══════════════════════════════════════════════════════════════════════════════

# colours
BG_DARK   = "#0D0D0D"
BG_PANEL  = "#141414"
BG_CARD   = "#1C1C1C"
BORDER    = "#2C2C2C"
ACCENT    = "#FF3B30"   # BlackCat red
ACCENT2   = "#FF9F0A"   # amber  (warnings / speed)
TEXT_PRI  = "#F0F0F0"
TEXT_SEC  = "#888888"
GREEN     = "#30D158"
RED       = "#FF3B30"
GREY_BLK  = "#2A2A2A"   # unscanned block

# disk-map grid
GRID_COLS  = 30
BLOCK_PX   = 22
MAX_BLOCKS = 600        # visual cap; actual block count may be larger


# ══════════════════════════════════════════════════════════════════════════════
#  LANGUAGES
# ══════════════════════════════════════════════════════════════════════════════

LANGS: dict[str, dict[str, str]] = {
    "EN": {
        "admin_warn":    "Run as Administrator for real disk access",
        "cfg_title":     "CONFIGURATION",
        "select_drive":  "Select Drive",
        "scan_mode":     "Scan Mode",
        "mode_quick":    "Quick  (128 MB blocks)",
        "mode_full":     "Full  (512 KB blocks)",
        "on_bad":        "On Bad Sector",
        "stop_first":    "Stop on first bad",
        "scan_finish":   "Scan until finish",
        "btn_start":     "START SCAN",
        "btn_stop":      "STOP",
        "status_title":  "STATUS",
        "ready":         "Ready",
        "map_title":     "DISK MAP",
        "leg_unscan":    "Not scanned",
        "leg_ok":        "OK",
        "leg_bad":       "Bad",
        "log_title":     "EVENT LOG",
        "log_ready":     "Ready — select a drive and press START SCAN.",
        "scanning":      "Scanning Disk {idx}  [{mode} mode]",
        "log_start":     "Disk {idx} | mode={mode} | stop_on_bad={sob}",
        "starting":      "Starting...",
        "speed_init":    "Speed: —",
        "bad_zero":      "Bad: 0",
        "bad_n":         "Bad: {n}",
        "speed_val":     "Speed: {v:.1f} MB/s",
        "stopping":      "Stopping...",
        "stop_user":     "Stop requested by user",
        "dlg_title":     "Confirm Stop",
        "dlg_body":      "Do you really want to stop the scan?\nProgress so far will not be lost.",
        "dlg_cancel":    "Cancel",
        "dlg_confirm":   "Stop Scan",
        "res_ok":        "Scan complete — No bad sectors found",
        "res_errors":    "Scan complete — {n} bad sector(s) found",
        "res_failed":    "Stopped — Bad sector detected ({n} total)",
        "res_stopped":   "Scan stopped by user",
        "res_admin":     "Permission denied — Run as Administrator",
        "res_size":      "Cannot read disk size — check disk index",
        "elapsed_log":   "elapsed: {t}",
        "admin_ok":      "Running as Administrator",
        "admin_no":      "NOT Administrator — scan will fail",
        "btn_relaunch":  "Relaunch as Admin",
    },
    "TH": {
        "admin_warn":    "กรุณารันในฐานะ Administrator",
        "cfg_title":     "ตั้งค่า",
        "select_drive":  "เลือกไดรฟ์",
        "scan_mode":     "โหมดสแกน",
        "mode_quick":    "Quick  (128 MB ต่อบล็อก)",
        "mode_full":     "Full  (512 KB ต่อบล็อก)",
        "on_bad":        "เมื่อพบ Bad Sector",
        "stop_first":    "หยุดทันทีที่พบครั้งแรก",
        "scan_finish":   "สแกนต่อจนจบ",
        "btn_start":     "เริ่มสแกน",
        "btn_stop":      "หยุด",
        "status_title":  "สถานะ",
        "ready":         "พร้อมใช้งาน",
        "map_title":     "แผนที่ดิสก์",
        "leg_unscan":    "ยังไม่สแกน",
        "leg_ok":        "ปกติ",
        "leg_bad":       "เสีย",
        "log_title":     "บันทึกเหตุการณ์",
        "log_ready":     "พร้อม — เลือกไดรฟ์และกดเริ่มสแกน",
        "scanning":      "กำลังสแกน Disk {idx}  [{mode}]",
        "log_start":     "Disk {idx} | mode={mode} | stop_on_bad={sob}",
        "starting":      "กำลังเริ่ม...",
        "speed_init":    "ความเร็ว: —",
        "bad_zero":      "เสีย: 0",
        "bad_n":         "เสีย: {n}",
        "speed_val":     "ความเร็ว: {v:.1f} MB/s",
        "stopping":      "กำลังหยุด...",
        "stop_user":     "ผู้ใช้สั่งหยุดการสแกน",
        "dlg_title":     "ยืนยันการหยุดสแกน",
        "dlg_body":      "ต้องการหยุดการสแกนจริงหรือไม่?\nข้อมูลที่สแกนไปแล้วจะไม่หาย",
        "dlg_cancel":    "ยกเลิก",
        "dlg_confirm":   "หยุดสแกน",
        "res_ok":        "สแกนเสร็จ — ไม่พบ Bad Sector",
        "res_errors":    "สแกนเสร็จ — พบ {n} Bad Sector",
        "res_failed":    "หยุดแล้ว — พบ Bad Sector ({n} จุด)",
        "res_stopped":   "ผู้ใช้หยุดการสแกน",
        "res_admin":     "ไม่มีสิทธิ์ — รันในฐานะ Administrator",
        "res_size":      "อ่านขนาดดิสก์ไม่ได้ — ตรวจสอบหมายเลขดิสก์",
        "elapsed_log":   "ใช้เวลา: {t}",
        "admin_ok":      "รันในฐานะ Administrator แล้ว",
        "admin_no":      "ไม่ใช่ Administrator — การสแกนจะล้มเหลว",
        "btn_relaunch":  "รีสตาร์ทในฐานะ Admin",
    },
    "JA": {
        "admin_warn":    "管理者として実行してください",
        "cfg_title":     "設定",
        "select_drive":  "ドライブ選択",
        "scan_mode":     "スキャンモード",
        "mode_quick":    "クイック  (128 MBブロック)",
        "mode_full":     "フル  (512 KBブロック)",
        "on_bad":        "不良セクタ検出時",
        "stop_first":    "最初の検出で停止",
        "scan_finish":   "最後までスキャン",
        "btn_start":     "スキャン開始",
        "btn_stop":      "停止",
        "status_title":  "ステータス",
        "ready":         "準備完了",
        "map_title":     "ディスクマップ",
        "leg_unscan":    "未スキャン",
        "leg_ok":        "正常",
        "leg_bad":       "不良",
        "log_title":     "イベントログ",
        "log_ready":     "準備完了 — ドライブを選んでスキャン開始を押してください",
        "scanning":      "Disk {idx} スキャン中  [{mode}]",
        "log_start":     "Disk {idx} | mode={mode} | stop_on_bad={sob}",
        "starting":      "開始中...",
        "speed_init":    "速度: —",
        "bad_zero":      "不良: 0",
        "bad_n":         "不良: {n}",
        "speed_val":     "速度: {v:.1f} MB/s",
        "stopping":      "停止中...",
        "stop_user":     "ユーザーがスキャンを停止しました",
        "dlg_title":     "停止の確認",
        "dlg_body":      "スキャンを本当に停止しますか?\nここまでの結果は保持されます。",
        "dlg_cancel":    "キャンセル",
        "dlg_confirm":   "停止する",
        "res_ok":        "スキャン完了 — 不良セクタなし",
        "res_errors":    "スキャン完了 — 不良セクタ {n} 個",
        "res_failed":    "停止 — 不良セクタ検出 ({n} 個)",
        "res_stopped":   "ユーザーがスキャンを停止",
        "res_admin":     "権限エラー — 管理者として実行してください",
        "res_size":      "ディスクサイズ読み取り失敗",
        "elapsed_log":   "経過時間: {t}",
        "admin_ok":      "管理者として実行中",
        "admin_no":      "管理者ではありません — スキャン失敗します",
        "btn_relaunch":  "管理者で再起動",
    },
    "ZH": {
        "admin_warn":    "请以管理员身份运行",
        "cfg_title":     "配置",
        "select_drive":  "选择驱动器",
        "scan_mode":     "扫描模式",
        "mode_quick":    "快速  (128 MB 块)",
        "mode_full":     "完整  (512 KB 块)",
        "on_bad":        "发现坏扇区时",
        "stop_first":    "发现即停止",
        "scan_finish":   "继续扫描至结束",
        "btn_start":     "开始扫描",
        "btn_stop":      "停止",
        "status_title":  "状态",
        "ready":         "就绪",
        "map_title":     "磁盘映射",
        "leg_unscan":    "未扫描",
        "leg_ok":        "正常",
        "leg_bad":       "坏块",
        "log_title":     "事件日志",
        "log_ready":     "就绪 — 选择驱动器并按开始扫描",
        "scanning":      "正在扫描 Disk {idx}  [{mode}]",
        "log_start":     "Disk {idx} | mode={mode} | stop_on_bad={sob}",
        "starting":      "正在启动...",
        "speed_init":    "速度: —",
        "bad_zero":      "坏块: 0",
        "bad_n":         "坏块: {n}",
        "speed_val":     "速度: {v:.1f} MB/s",
        "stopping":      "正在停止...",
        "stop_user":     "用户请求停止扫描",
        "dlg_title":     "确认停止",
        "dlg_body":      "确定要停止扫描吗?\n已完成的进度不会丢失。",
        "dlg_cancel":    "取消",
        "dlg_confirm":   "停止扫描",
        "res_ok":        "扫描完成 — 未发现坏扇区",
        "res_errors":    "扫描完成 — 发现 {n} 个坏扇区",
        "res_failed":    "已停止 — 检测到坏扇区 ({n} 个)",
        "res_stopped":   "用户停止扫描",
        "res_admin":     "权限不足 — 请以管理员身份运行",
        "res_size":      "无法读取磁盘大小",
        "elapsed_log":   "耗时: {t}",
        "admin_ok":      "正在以管理员身份运行",
        "admin_no":      "非管理员 — 扫描将失败",
        "btn_relaunch":  "以管理员重新启动",
    },
}


# ══════════════════════════════════════════════════════════════════════════════
#  DISK HELPERS
# ══════════════════════════════════════════════════════════════════════════════

def get_disk_size(disk_index: int) -> int:
    """Return byte size of PhysicalDrive{disk_index} via DeviceIoControl.
    Returns 0 on any failure (no access, not found, non-Windows)."""
    if platform.system() != "Windows":
        return 0

    IOCTL_DISK_GET_DRIVE_GEOMETRY_EX = 0x000700A0
    path = f"\\\\.\\PhysicalDrive{disk_index}"

    class _Geo(ctypes.Structure):
        _fields_ = [
            ("Cylinders",         ctypes.c_longlong),
            ("MediaType",         ctypes.c_uint),
            ("TracksPerCylinder", ctypes.c_ulong),
            ("SectorsPerTrack",   ctypes.c_ulong),
            ("BytesPerSector",    ctypes.c_ulong),
        ]

    class _GeoEx(ctypes.Structure):
        _fields_ = [
            ("Geometry", _Geo),
            ("DiskSize", ctypes.c_longlong),
            ("Data",     ctypes.c_byte * 1),
        ]

    try:
        hnd = ctypes.windll.kernel32.CreateFileW(
            path, 0x80000000, 0x3, None, 3, 0, None)
        if hnd == ctypes.wintypes.HANDLE(-1).value:
            return 0
        geo    = _GeoEx()
        br     = ctypes.c_ulong(0)
        ok     = ctypes.windll.kernel32.DeviceIoControl(
            hnd, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
            None, 0, ctypes.byref(geo), ctypes.sizeof(geo),
            ctypes.byref(br), None)
        ctypes.windll.kernel32.CloseHandle(hnd)
        return geo.DiskSize if ok else 0
    except Exception:
        return 0


def _parse_pipe_lines(stdout: str) -> list[dict]:
    """Parse 'id|name|size' lines from PowerShell output into drive dicts."""
    drives = []
    for line in stdout.splitlines():
        line = line.strip()
        if not line or "|" not in line:
            continue
        parts = line.split("|", 2)
        if len(parts) < 3:
            continue
        id_s, model, size_s = parts[0].strip(), parts[1].strip(), parts[2].strip()
        if not id_s.isdigit():
            continue
        idx = int(id_s)
        sb  = int(size_s) if size_s.isdigit() else 0
        gb  = f"{round(sb / 1024**3, 1)} GB" if sb else "size unknown"
        drives.append({
            "index":      idx,
            "label":      f"Disk {idx}  —  {model or f'Disk {idx}'}  ({gb})",
            "size_bytes": sb,
        })
    return drives


# Windows flag that prevents console windows from flashing on subprocess launch
_NO_WINDOW = subprocess.CREATE_NO_WINDOW if platform.system() == "Windows" else 0


def _run_ps(command: str, timeout: int = 12) -> str:
    """Run a hidden PowerShell command and return stdout (empty on failure)."""
    try:
        r = subprocess.run(
            ["powershell", "-NoProfile", "-NonInteractive", "-Command", command],
            capture_output=True, text=True,
            timeout=timeout, encoding="utf-8", errors="replace",
            creationflags=_NO_WINDOW)
        return r.stdout
    except Exception:
        return ""


def list_drives() -> list[dict]:
    """Detect all physical drives.  Four fallback methods, best-effort."""
    if platform.system() != "Windows":
        # Demo drives for development on non-Windows machines
        demo = [
            ("Samsung SSD 870 EVO 500GB", 500),
            ("WD Blue 1TB SATA HDD",      1000),
            ("Seagate Barracuda 2TB",      2000),
        ]
        return [
            {"index": i, "label": f"Disk {i}  —  {m}  ({s} GB)",
             "size_bytes": s * 1024**3}
            for i, (m, s) in enumerate(demo)
        ]

    drives: list[dict] = []

    # 1 — Get-PhysicalDisk (most complete: NVMe, USB, SD cards)
    ps1 = (
        "Get-PhysicalDisk | Sort-Object DeviceId | "
        "Select-Object DeviceId,FriendlyName,Size | "
        "ForEach-Object { $_.DeviceId + '|' + $_.FriendlyName + '|' + $_.Size }"
    )
    drives = _parse_pipe_lines(_run_ps(ps1))

    # 2 — Get-Disk (partition-visible disks)
    if not drives:
        ps2 = (
            "Get-Disk | Sort-Object Number | "
            "Select-Object Number,FriendlyName,Size | "
            "ForEach-Object { $_.Number.ToString() + '|' + $_.FriendlyName + '|' + $_.Size.ToString() }"
        )
        drives = _parse_pipe_lines(_run_ps(ps2))

    # 3 — wmic /format:list (works on older Windows without PS remoting)
    if not drives:
        try:
            r = subprocess.run(
                ["wmic", "diskdrive", "get", "Index,Model,Size", "/format:list"],
                capture_output=True, timeout=10,
                encoding="utf-8", errors="replace",
                creationflags=_NO_WINDOW)
            current: dict[str, str] = {}
            for line in r.stdout.splitlines():
                line = line.strip()
                if "=" in line:
                    k, v = line.split("=", 1)
                    current[k.strip()] = v.strip()
                elif current:
                    if current.get("Index", "").isdigit():
                        idx   = int(current["Index"])
                        model = current.get("Model", "").strip() or f"Disk {idx}"
                        sb    = int(current["Size"]) if current.get("Size", "").isdigit() else 0
                        gb    = f"{round(sb / 1024**3, 1)} GB" if sb else "size unknown"
                        drives.append({"index": idx,
                                       "label": f"Disk {idx}  —  {model}  ({gb})",
                                       "size_bytes": sb})
                    current = {}
        except Exception:
            pass

    # 4 — brute-force ctypes probe (guaranteed last-resort)
    if not drives:
        for i in range(10):
            sz = get_disk_size(i)
            if sz > 0:
                drives.append({
                    "index":      i,
                    "label":      f"Disk {i}  —  PhysicalDrive{i}  ({round(sz/1024**3,1)} GB)",
                    "size_bytes": sz,
                })

    drives.sort(key=lambda d: d["index"])
    return drives


# ══════════════════════════════════════════════════════════════════════════════
#  SCAN ENGINE  (runs in a background thread)
# ══════════════════════════════════════════════════════════════════════════════

def scan_disk(
    disk_index:  int,
    mode:        str,           # "quick" | "full"
    stop_on_bad: bool,
    on_block:    callable,      # (index: int, status: "green"|"red") -> None
    on_progress: callable,      # (scanned_mb, total_mb, bad_count, speed_mbs) -> None
    on_finish:   callable,      # (result: str, bad_count: int) -> None
    stop_event:  threading.Event,
) -> None:
    """
    Read-only sector scan.  Uses open(path, "rb") so no data is written.
    OSError on a read = bad sector.  All UI callbacks are fire-and-forget;
    the caller must ensure thread-safety (Flet's page.update() handles this).
    """
    MB = 1024 * 1024
    block_size = 128 * MB if mode == "quick" else 512 * 1024

    # ── demo / non-Windows simulation ────────────────────────────────────────
    if platform.system() != "Windows":
        total_demo  = 200
        bad_count   = 0
        for i in range(total_demo):
            if stop_event.is_set():
                on_finish("STOPPED", bad_count)
                return
            time.sleep(0.04)
            is_bad = random.random() < 0.04
            if is_bad:
                bad_count += 1
                on_block(i, "red")
                if stop_on_bad:
                    on_progress(i * block_size // MB,
                                total_demo * block_size // MB,
                                bad_count, 80.0)
                    on_finish("FAILED", bad_count)
                    return
            else:
                on_block(i, "green")
            on_progress((i + 1) * block_size // MB,
                        total_demo * block_size // MB,
                        bad_count, 80.0 + i * 0.1)
        on_finish("SUCCESS", bad_count)
        return

    # ── real scan (Windows, raw disk read) ───────────────────────────────────
    disk_path  = f"\\\\.\\PhysicalDrive{disk_index}"
    disk_total = get_disk_size(disk_index)
    if not disk_total:
        on_finish("ERROR_SIZE", 0)
        return

    pos       = 0
    blk_idx   = 0
    bad_count = 0
    t0        = time.time()

    try:
        with open(disk_path, "rb") as disk:
            while pos < disk_total:
                if stop_event.is_set():
                    on_finish("STOPPED", bad_count)
                    return

                try:
                    disk.seek(pos)
                    data = disk.read(block_size)
                    if not data:
                        break
                    on_block(blk_idx, "green")
                except OSError:
                    bad_count += 1
                    on_block(blk_idx, "red")
                    if stop_on_bad:
                        elapsed = max(0.001, time.time() - t0)
                        on_progress(pos // MB, disk_total // MB,
                                    bad_count, (pos // MB) / elapsed)
                        on_finish("FAILED", bad_count)
                        return

                elapsed = max(0.001, time.time() - t0)
                on_progress(pos // MB, disk_total // MB,
                            bad_count, (pos // MB) / elapsed)
                pos     += block_size
                blk_idx += 1

        result = "SUCCESS" if bad_count == 0 else "DONE_WITH_ERRORS"
        on_finish(result, bad_count)

    except PermissionError:
        on_finish("ERROR_ADMIN", bad_count)
    except Exception as exc:
        on_finish(f"ERROR: {exc}", bad_count)


# ══════════════════════════════════════════════════════════════════════════════
#  FLET UI
# ══════════════════════════════════════════════════════════════════════════════

def main(page: ft.Page) -> None:
    page.title         = "BlackCat Disk Scanner"
    page.bgcolor       = BG_DARK
    page.theme_mode    = ft.ThemeMode.DARK
    page.window.width  = 960
    page.window.height = 760
    page.padding       = 0

    drives     = list_drives()
    admin_mode = is_admin()   # checked once at startup

    # ── app state ─────────────────────────────────────────────────────────────
    scan_thread:  threading.Thread | None = None
    timer_thread: threading.Thread | None = None
    stop_event    = threading.Event()
    scan_start_t  = [0.0]   # list so inner functions can rebind the value
    cur_lang      = ["EN"]  # same trick

    def T(key: str) -> str:
        return LANGS[cur_lang[0]].get(key, key)

    # ── helpers ───────────────────────────────────────────────────────────────
    def fmt_elapsed() -> str:
        secs = int(time.time() - scan_start_t[0])
        h, rem = divmod(secs, 3600)
        m, s   = divmod(rem, 60)
        return f"{h:02d}:{m:02d}:{s:02d}"

    def add_log(msg: str, color: str = TEXT_SEC) -> None:
        ts = time.strftime("%H:%M:%S")
        log_list.controls.append(
            ft.Text(f"[{ts}]  {msg}", size=13, color=color,
                    font_family="Monospace"))
        if len(log_list.controls) > 300:
            log_list.controls.pop(0)
        page.update()

    # ── language switcher ─────────────────────────────────────────────────────
    lang_order = ["EN", "TH", "JA", "ZH"]
    lang_btns: dict[str, ft.TextButton] = {}

    def _make_lang_btn(code: str) -> ft.TextButton:
        def _on_click(_):
            cur_lang[0] = code
            apply_lang()
        btn = ft.TextButton(
            code, on_click=_on_click,
            style=ft.ButtonStyle(
                color={"": ACCENT if code == cur_lang[0] else TEXT_SEC},
                padding=ft.Padding(10, 6, 10, 6),
                text_style=ft.TextStyle(size=14, weight=ft.FontWeight.W_500),
            ))
        lang_btns[code] = btn
        return btn

    lang_row = ft.Row([_make_lang_btn(c) for c in lang_order], spacing=2)

    # ── form controls ─────────────────────────────────────────────────────────
    dd_drive = ft.Dropdown(
        label="Select Drive",
        options=[ft.dropdown.Option(str(d["index"]), d["label"]) for d in drives],
        value=str(drives[0]["index"]) if drives else "0",
        width=370,
        bgcolor=BG_CARD,
        border_color=BORDER,
        color=TEXT_PRI,
        label_style=ft.TextStyle(color=TEXT_SEC, size=13),
        text_style=ft.TextStyle(color=TEXT_PRI,  size=14),
    )

    rb_quick      = ft.Radio(value="quick",    label_style=ft.TextStyle(color=TEXT_PRI, size=14))
    rb_full       = ft.Radio(value="full",     label_style=ft.TextStyle(color=TEXT_PRI, size=14))
    rb_stop_first = ft.Radio(value="stop",     label_style=ft.TextStyle(color=TEXT_PRI, size=14))
    rb_continue   = ft.Radio(value="continue", label_style=ft.TextStyle(color=TEXT_PRI, size=14))

    rg_mode = ft.RadioGroup(value="quick",
                             content=ft.Row([rb_quick, rb_full], spacing=20))
    rg_stop = ft.RadioGroup(value="stop",
                             content=ft.Row([rb_stop_first, rb_continue], spacing=20))

    # ── status / progress widgets ─────────────────────────────────────────────
    txt_status   = ft.Text("", color=TEXT_SEC, size=15)
    txt_progress = ft.Text("—", color=TEXT_PRI, size=14)
    txt_bad      = ft.Text("", color=RED,    size=14, weight=ft.FontWeight.W_500)
    txt_speed    = ft.Text("", color=ACCENT2, size=14)
    txt_elapsed  = ft.Text("00:00:00", color=TEXT_PRI, size=15,
                            weight=ft.FontWeight.W_500, font_family="Monospace")
    prog_bar     = ft.ProgressBar(value=0, bgcolor=BG_CARD, color=ACCENT, height=5)

    # ── translatable labels ───────────────────────────────────────────────────
    lbl_cfg       = ft.Text("", size=13, color=TEXT_SEC, weight=ft.FontWeight.W_500)
    lbl_status    = ft.Text("", size=13, color=TEXT_SEC, weight=ft.FontWeight.W_500)
    lbl_map       = ft.Text("", size=13, color=TEXT_SEC, weight=ft.FontWeight.W_500)
    lbl_log       = ft.Text("", size=13, color=TEXT_SEC, weight=ft.FontWeight.W_500)
    lbl_scan_mode = ft.Text("", size=13, color=TEXT_SEC)
    lbl_on_bad    = ft.Text("", size=13, color=TEXT_SEC)
    # admin status badge — icon + text, colour driven by admin_mode
    _admin_icon  = ft.Text("✓" if admin_mode else "✗",
                            size=14, weight=ft.FontWeight.W_500,
                            color=GREEN if admin_mode else RED)
    txt_admin    = ft.Text("", size=13,
                            color=GREEN if admin_mode else RED,
                            italic=not admin_mode,
                            weight=ft.FontWeight.W_500 if admin_mode else ft.FontWeight.W_400)

    _btn_relaunch = ft.TextButton(
        "",
        visible=not admin_mode,
        on_click=lambda _: relaunch_as_admin(),
        style=ft.ButtonStyle(
            color={"": ACCENT},
            padding=ft.Padding(8, 4, 8, 4),
            text_style=ft.TextStyle(size=13, weight=ft.FontWeight.W_500),
        ),
    )

    admin_row = ft.Row(
        [_admin_icon, txt_admin, _btn_relaunch],
        spacing=6,
        vertical_alignment=ft.CrossAxisAlignment.CENTER,
    )
    dot_unscan    = ft.Text("", size=13, color=TEXT_SEC)
    dot_ok        = ft.Text("", size=13, color=TEXT_SEC)
    dot_bad_lbl   = ft.Text("", size=13, color=TEXT_SEC)

    # ── buttons — use ElevatedButton + explicit Text child for reliable labels ─
    btn_start_lbl = ft.Text("", color="#FFFFFF", size=15, weight=ft.FontWeight.W_500)
    btn_stop_lbl  = ft.Text("", color=ACCENT,   size=15, weight=ft.FontWeight.W_500)

    btn_start = ft.ElevatedButton(
        content=ft.Row([btn_start_lbl], tight=True),
        on_click=lambda _: _start_scan(),
        style=ft.ButtonStyle(
            bgcolor={"": ACCENT, "disabled": "#3A1010"},
            overlay_color={"": "#CC2A22"},
            shape=ft.RoundedRectangleBorder(radius=8),
            padding=ft.Padding(24, 14, 24, 14),
        ))

    btn_stop = ft.ElevatedButton(
        content=ft.Row([btn_stop_lbl], tight=True),
        on_click=lambda _: _confirm_stop(),
        disabled=True,
        style=ft.ButtonStyle(
            bgcolor={"": BG_CARD, "disabled": BG_CARD},
            overlay_color={"": "#2A0A0A"},
            side={"": ft.BorderSide(1, ACCENT),
                  "disabled": ft.BorderSide(1, "#5A2020")},
            shape=ft.RoundedRectangleBorder(radius=8),
            padding=ft.Padding(20, 14, 20, 14),
        ))

    # ── disk-map grid ─────────────────────────────────────────────────────────
    grid_cells: list[ft.Container] = [
        ft.Container(width=BLOCK_PX, height=BLOCK_PX,
                     bgcolor=GREY_BLK, border_radius=3)
        for _ in range(MAX_BLOCKS)
    ]
    grid_view = ft.GridView(
        controls=grid_cells,
        runs_count=GRID_COLS,
        max_extent=BLOCK_PX,
        child_aspect_ratio=1.0,
        spacing=3, run_spacing=3,
        expand=True,
    )

    # ── event log ─────────────────────────────────────────────────────────────
    log_list = ft.ListView(expand=True, spacing=1, auto_scroll=True)

    # ── scan callbacks (called from background thread) ────────────────────────
    def _on_block(index: int, status: str) -> None:
        vi = min(index, MAX_BLOCKS - 1)
        grid_cells[vi].bgcolor = GREEN if status == "green" else RED
        page.update()

    def _on_progress(scanned_mb: int, total_mb: int,
                     bad_count: int, speed: float) -> None:
        pct = scanned_mb / total_mb if total_mb else 0
        prog_bar.value     = min(pct, 1.0)
        txt_progress.value = f"{scanned_mb:,} / {total_mb:,} MB  ({pct*100:.1f}%)"
        txt_bad.value      = T("bad_n").format(n=bad_count)
        txt_speed.value    = T("speed_val").format(v=speed)
        txt_elapsed.value  = fmt_elapsed()
        page.update()

    def _on_finish(result: str, bad_count: int) -> None:
        nonlocal scan_thread, timer_thread
        scan_thread = timer_thread = None

        btn_start.disabled = False
        btn_stop.disabled  = True
        dd_drive.disabled  = False
        rg_mode.disabled   = False
        rg_stop.disabled   = False

        result_map = {
            "SUCCESS":          (T("res_ok"),                             GREEN),
            "DONE_WITH_ERRORS": (T("res_errors").format(n=bad_count),     ACCENT2),
            "FAILED":           (T("res_failed").format(n=bad_count),     RED),
            "STOPPED":          (T("res_stopped"),                        ACCENT2),
            "ERROR_ADMIN":      (T("res_admin"),                          RED),
            "ERROR_SIZE":       (T("res_size"),                           RED),
        }
        msg, color = result_map.get(result, (result, TEXT_SEC))
        txt_status.value = msg
        txt_status.color = color
        add_log(f"{msg}  |  {T('elapsed_log').format(t=fmt_elapsed())}", color)
        page.update()

    # ── timer tick (separate thread so it ticks even between progress calls) ──
    def _timer_tick(sentinel: threading.Thread) -> None:
        """Tick once per second while `sentinel` thread is alive."""
        while sentinel.is_alive():
            time.sleep(1)
            txt_elapsed.value = fmt_elapsed()
            try:
                page.update()
            except Exception:
                break

    # ── start scan ────────────────────────────────────────────────────────────
    def _start_scan() -> None:
        nonlocal scan_thread, timer_thread, stop_event

        if scan_thread and scan_thread.is_alive():
            return

        idx         = int(dd_drive.value or 0)
        mode        = rg_mode.value
        stop_on_bad = rg_stop.value == "stop"

        # reset state
        stop_event      = threading.Event()
        scan_start_t[0] = time.time()

        for cell in grid_cells:
            cell.bgcolor = GREY_BLK
        log_list.controls.clear()
        prog_bar.value     = 0
        txt_progress.value = "—"
        txt_bad.value      = T("bad_zero")
        txt_speed.value    = T("speed_init")
        txt_elapsed.value  = "00:00:00"
        txt_status.value   = T("scanning").format(idx=idx, mode=mode)
        txt_status.color   = ACCENT2

        btn_start.disabled = True
        btn_stop.disabled  = False
        dd_drive.disabled  = True
        rg_mode.disabled   = True
        rg_stop.disabled   = True

        add_log(T("log_start").format(idx=idx, mode=mode, sob=stop_on_bad), ACCENT2)
        page.update()

        scan_thread = threading.Thread(
            target=scan_disk,
            args=(idx, mode, stop_on_bad,
                  _on_block, _on_progress, _on_finish, stop_event),
            daemon=True, name=f"scan-disk{idx}")
        timer_thread = threading.Thread(
            target=_timer_tick,
            args=(scan_thread,),
            daemon=True, name="scan-timer")
        scan_thread.start()
        timer_thread.start()

    # ── confirm stop dialog ───────────────────────────────────────────────────
    def _confirm_stop() -> None:

        def _do_stop(_) -> None:
            confirm_dlg.open = False
            page.update()
            stop_event.set()
            txt_status.value  = T("stopping")
            txt_status.color  = ACCENT2
            btn_stop.disabled = True
            add_log(T("stop_user"), ACCENT2)
            page.update()

        def _cancel(_) -> None:
            confirm_dlg.open = False
            page.update()

        confirm_dlg = ft.AlertDialog(
            modal=True, open=True,
            title=ft.Text(T("dlg_title"), color=TEXT_PRI,
                          weight=ft.FontWeight.W_500, size=16),
            content=ft.Text(T("dlg_body"), color=TEXT_SEC, size=14),
            bgcolor=BG_CARD,
            actions=[
                ft.TextButton(T("dlg_cancel"), on_click=_cancel,
                              style=ft.ButtonStyle(
                                  color={"": TEXT_SEC},
                                  text_style=ft.TextStyle(size=14))),
                ft.FilledButton(
                    T("dlg_confirm"), on_click=_do_stop,
                    style=ft.ButtonStyle(
                        bgcolor={"": ACCENT},
                        color={"": "#FFFFFF"},
                        shape=ft.RoundedRectangleBorder(radius=6),
                        text_style=ft.TextStyle(size=14))),
            ],
            actions_alignment=ft.MainAxisAlignment.END,
        )
        page.overlay.append(confirm_dlg)
        page.update()

    # ── apply language (updates every translatable widget) ───────────────────
    def apply_lang() -> None:
        for code, btn in lang_btns.items():
            btn.style = ft.ButtonStyle(
                color={"": ACCENT if code == cur_lang[0] else TEXT_SEC},
                padding=ft.Padding(10, 6, 10, 6),
                text_style=ft.TextStyle(size=14, weight=ft.FontWeight.W_500),
            )

        lbl_cfg.value        = T("cfg_title")
        lbl_status.value     = T("status_title")
        lbl_map.value        = T("map_title")
        lbl_log.value        = T("log_title")
        # admin badge text — depends on runtime admin_mode (not just a warning)
        txt_admin.value       = T("admin_ok") if admin_mode else T("admin_no")
        _btn_relaunch.text    = T("btn_relaunch")
        lbl_scan_mode.value  = T("scan_mode")
        lbl_on_bad.value     = T("on_bad")
        dd_drive.label       = T("select_drive")
        rb_quick.label       = T("mode_quick")
        rb_full.label        = T("mode_full")
        rb_stop_first.label  = T("stop_first")
        rb_continue.label    = T("scan_finish")
        btn_start_lbl.value  = f"▶  {T('btn_start')}"
        btn_stop_lbl.value   = f"■  {T('btn_stop')}"
        dot_unscan.value     = T("leg_unscan")
        dot_ok.value         = T("leg_ok")
        dot_bad_lbl.value    = T("leg_bad")

        if not (scan_thread and scan_thread.is_alive()):
            txt_status.value = T("ready")
            txt_bad.value    = T("bad_zero")
            txt_speed.value  = T("speed_init")

        page.update()

    # ── UI helpers ────────────────────────────────────────────────────────────
    def _dot_row(color: str, label_widget: ft.Text) -> ft.Row:
        return ft.Row([
            ft.Container(width=12, height=12, bgcolor=color, border_radius=2),
            label_widget,
        ], spacing=6)

    def _panel(header_widget: ft.Text, *children,
               width=None, height=None, expand=False) -> ft.Container:
        return ft.Container(
            content=ft.Column(
                [header_widget,
                 ft.Divider(height=1, color=BORDER),
                 *children],
                spacing=10, expand=expand),
            bgcolor=BG_PANEL,
            border=ft.Border(
                top=ft.BorderSide(1, BORDER), bottom=ft.BorderSide(1, BORDER),
                left=ft.BorderSide(1, BORDER), right=ft.BorderSide(1, BORDER)),
            border_radius=8,
            padding=ft.Padding(18, 14, 18, 14),
            width=width, height=height, expand=expand,
        )

    # ── layout ────────────────────────────────────────────────────────────────
    timer_box = ft.Container(
        content=ft.Row([ft.Text("⏱", size=14), txt_elapsed], spacing=6),
        bgcolor=BG_CARD,
        border_radius=6,
        padding=ft.Padding(12, 8, 12, 8),
        border=ft.Border(
            top=ft.BorderSide(1, BORDER), bottom=ft.BorderSide(1, BORDER),
            left=ft.BorderSide(1, BORDER), right=ft.BorderSide(1, BORDER)),
    )

    config_panel = _panel(
        lbl_cfg,
        dd_drive,
        ft.Column([lbl_scan_mode, rg_mode], spacing=4),
        ft.Column([lbl_on_bad,    rg_stop],  spacing=4),
        ft.Row([btn_start, btn_stop], spacing=12),
        width=390,
    )

    status_panel = _panel(
        lbl_status,
        txt_status,
        prog_bar,
        ft.Row([txt_progress,
                ft.Container(expand=True),
                txt_speed, txt_bad, timer_box],
               spacing=8,
               vertical_alignment=ft.CrossAxisAlignment.CENTER),
        expand=True,
    )

    map_panel = _panel(
        lbl_map,
        ft.Row([ft.Container(expand=True),
                _dot_row(GREY_BLK, dot_unscan),
                _dot_row(GREEN,    dot_ok),
                _dot_row(RED,      dot_bad_lbl)],
               spacing=14),
        ft.Container(content=grid_view, expand=True),
        height=300, expand=True,
    )

    log_panel = _panel(
        lbl_log,
        ft.Container(content=log_list, height=140),
    )

    header = ft.Container(
        content=ft.Row([
            ft.Column([
                ft.Text("BlackCat", size=26,
                        weight=ft.FontWeight.BOLD, color=ACCENT),
                ft.Text("Disk Scanner  v3.1", size=13, color=TEXT_SEC),
            ], spacing=1),
            ft.Container(expand=True),
            admin_row,
            ft.Container(width=16),
            lang_row,
        ]),
        bgcolor=BG_PANEL,
        padding=ft.Padding(24, 12, 24, 12),
        border=ft.Border(bottom=ft.BorderSide(1, BORDER)),
    )

    body = ft.Container(
        content=ft.Column([
            ft.Row([config_panel, status_panel],
                   spacing=14,
                   alignment=ft.MainAxisAlignment.START,
                   vertical_alignment=ft.CrossAxisAlignment.START),
            map_panel,
            log_panel,
        ], spacing=14),
        padding=ft.Padding(20, 18, 20, 18),
        expand=True,
    )

    page.add(ft.Column([header, body], spacing=0, expand=True))

    # first render with default language
    apply_lang()

    # disable scan if not running as admin (Windows only)
    if not admin_mode and platform.system() == "Windows":
        btn_start.disabled = True
        add_log("⚠  Not running as Administrator — scan disabled.", RED)
    else:
        add_log(T("log_ready"), TEXT_SEC)

    page.update()


# ══════════════════════════════════════════════════════════════════════════════
#  ENTRY POINT
# ══════════════════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    ft.run(main)