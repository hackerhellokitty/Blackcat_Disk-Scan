# BlackCat Disk Scanner

เครื่องมือสแกน HDD / SSD ระดับ sector บน Windows พร้อม UI แบบ real-time

---

## Version

| Version | Language | Status |
|---------|----------|--------|
| **v4.2.0** | C (Win32 native) | ✅ Current |
| v4.1.0 | Python / Flet | Legacy |

---

## Build (C version — v4.2.0)

### Requirements

- Windows 10/11
- Visual Studio 2022+ (MSVC) หรือ VS 2026 Insiders
- CMake 3.20+
- Ninja (ถ้าไม่มีจะ fallback ไป Visual Studio generator)
- Internet connection (CMake จะ fetch **libharu** อัตโนมัติครั้งแรก)

### Build

```bat
build_msvc.bat
```

EXE อยู่ที่ `dist_c\blackcat_disk_scanner.exe` — standalone ไม่ต้องพก DLL

---

## Features

### การสแกน
| โหมด | Block Size | เหมาะกับ |
|------|-----------|---------|
| Quick | 128 MB / block | ตรวจสอบเบื้องต้นรวดเร็ว |
| Full | 512 KB / block | ตรวจสอบละเอียด ระบุตำแหน่ง bad sector แม่นยำกว่า 256x |

- ตรวจจับ **bad sector** จาก read error ระดับ OS (kernel32 `ReadFile`)
- บันทึก **byte offset** (decimal + hex) ลง **Windows Event Log** อัตโนมัติ
- เลือกพฤติกรรมเมื่อพบ bad sector: **หยุดทันที** หรือ **สแกนต่อจนจบ**
- **Demo Mode** — จำลองการสแกนแบบสุ่มโดยไม่อ่าน disk จริง

### UI
- Win32 native — ไม่ต้องติดตั้ง runtime ใด ๆ
- **Dark / Light theme** — dark โทนน้ำเงินเข้ม, light โทน warm-grey ไม่แสบตา
- **Disk Map** — grid แสดงสถานะแต่ละ block (เทา = ยังไม่สแกน, เขียว = OK, แดง = Bad) เต็มกรอบ
- **Progress bar** พร้อม MB scanned / total, ความเร็ว MB/s, จำนวน bad sector
- **Event Log** บันทึกเหตุการณ์พร้อม timestamp และ byte offset ของ bad sector
- สลับภาษา **EN / TH** ได้ทันที

### PDF Report
- สร้างรายงานผ่าน **libharu** (static link — ไม่ต้องพก DLL)
- รองรับ **ฟอนต์ภาษาไทย** (Leelawadee UI → Leelawadee → Tahoma)
- เนื้อหา: Scan Info, Statistics, Disk Map grid, Event Log
- แสดง Duration จริง, รองรับหลายหน้าอัตโนมัติ

### Drive Detection
ตรวจจับ drive อัตโนมัติผ่าน 3 วิธีเรียงลำดับ:
1. `Get-PhysicalDisk` (PowerShell)
2. `Get-Disk` (PowerShell)
3. IOCTL `IOCTL_DISK_GET_DRIVE_GEOMETRY_EX` (fallback)

---

## Project Structure

```
├── src/
│   ├── main.c          — WinMain, admin check
│   ├── admin.c/.h      — IsUserAnAdmin, ShellExecute runas
│   ├── disk.c/.h       — drive detection, scan engine (thread)
│   ├── eventlog.c/.h   — Windows Event Log (advapi32)
│   ├── i18n.c/.h       — EN/TH string tables
│   ├── pdf.c/.h        — PDF report (libharu)
│   ├── ui.c/.h         — Win32 GUI
│   ├── utils.c/.h      — format helpers
│   └── app.rc          — icon + version info
├── assets/
│   └── ssd.ico
├── CMakeLists.txt
├── build_msvc.bat
└── disk_scan.py        — Python v4.1.0 (legacy)
```

---

## Changelog

### v4.2.0 — C rewrite
- **Win32 native** — เขียนใหม่ทั้งหมดด้วย C, ไม่พึ่ง Python หรือ Flet
- **Static binary** — EXE เดียว ไม่ต้องพก DLL (libharu static link)
- **Icon embed** — `ssd.ico` ฝังใน EXE ผ่าน `.rc` resource
- **Dark theme** ปรับโทนน้ำเงินเข้มอ่านสบายตา, **Light theme** ปรับ warm-grey ลดความจ้า
- **Disk Map** คำนวณ cols จากความกว้าง panel จริง — block เต็มกรอบ
- **PDF**: แก้ disk map ล้น A4, แก้ Duration แสดง 0s

### v4.1.0
- **Windows Event Log** — บันทึก byte offset (decimal + hex) เมื่อพบ bad sector
- **PDF ภาษาไทย** — แก้ตัวอักษรไทยแสดงเป็นกล่องสี่เหลี่ยม

### v4.0.0
- เพิ่ม **PDF Report** — Scan Info, Statistics, Disk Map, Event Log

### v3.2.0
- เพิ่ม **Dark / Light Theme**

### v3.1.0
- Initial release — sector scan, EN/TH, Quick/Full, Disk Map, Demo Mode

---

## License

Internal tool — BlackCat Project
