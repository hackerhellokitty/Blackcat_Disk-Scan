# BlackCat Disk Scanner

```
██████╗ ██╗      █████╗  ██████╗██╗  ██╗ ██████╗ █████╗ ████████╗
██╔══██╗██║     ██╔══██╗██╔════╝██║ ██╔╝██╔════╝██╔══██╗╚══██╔══╝
██████╔╝██║     ███████║██║     █████╔╝ ██║     ███████║   ██║
██╔══██╗██║     ██╔══██║██║     ██╔═██╗ ██║     ██╔══██║   ██║
██████╔╝███████╗██║  ██║╚██████╗██║  ██╗╚██████╗██║  ██║   ██║
╚═════╝ ╚══════╝╚═╝  ╚═╝ ╚═════╝╚═╝  ╚═╝ ╚═════╝╚═╝  ╚═╝   ╚═╝
```

**HDD / SSD Sector-Level Read Scanner** | Tool 22 | Kuroneko Tools

---

## ภาพรวม

BlackCat Disk Scanner สแกนทุก sector ของ HDD หรือ SSD โดยตรงผ่าน raw disk access เพื่อตรวจหา Bad Sector ก่อนที่ข้อมูลจะเสียหาย แสดงผลเป็น block map แบบ real-time พร้อม event log และ timer

---

## ความต้องการของระบบ

| รายการ | ข้อกำหนด |
|--------|----------|
| OS | Windows 10 / 11 |
| Python | 3.10 ขึ้นไป |
| Flet | >= 0.80 |
| สิทธิ์ | **Administrator** (จำเป็นสำหรับ raw disk access) |

```bash
pip install flet
```

---

## การรัน

### รันจาก Source

```bash
# รันในฐานะ Administrator เท่านั้น
python blackcat_disk_scanner.py
```

### Build เป็น EXE

```bash
# วางไฟล์ทั้งหมดในโฟลเดอร์เดียวกัน แล้วรัน
build_exe.bat
```

EXE จะอยู่ที่ `dist\blackcat_Disk_Scanner.exe`  
EXE นี้ฝัง `--uac-admin` ไว้แล้ว จะขอ UAC elevation อัตโนมัติเมื่อดับเบิลคลิก

---

## โครงสร้างโปรเจกต์

```
HDD Scanner/
├── blackcat_disk_scanner.py   ← โปรแกรมหลัก
├── build_exe.bat              ← สคริปต์ build EXE
├── README.md                  ← ไฟล์นี้
└── assets/
    └── ssd.ico                ← ไอคอน (optional)
```

---

## ฟีเจอร์

### การสแกน
- **Quick Scan** — อ่านทีละ 128 MB เร็ว เหมาะกับการตรวจเบื้องต้น
- **Full Scan** — อ่านทีละ 512 KB ละเอียด ตรวจทุก sector
- **Stop on first bad** — หยุดทันทีเมื่อพบ Bad Sector แรก
- **Scan until finish** — สแกนต่อจนครบ นับ Bad Sector ทั้งหมด
- Confirm dialog ก่อนหยุดสแกน ป้องกันกด stop ผิด

### UI
- **Block Map** — กล่อง 600 บล็อก แสดงสีแบบ real-time
  - ⬛ เทา = ยังไม่สแกน
  - 🟩 เขียว = อ่านผ่าน
  - 🟥 แดง = Bad Sector
- **Progress Bar** — แสดง MB ที่สแกนแล้ว / ทั้งหมด + เปอร์เซ็นต์
- **Speed Meter** — ความเร็วในการอ่าน MB/s
- **Timer** — นับเวลา HH:MM:SS แบบ real-time
- **Event Log** — บันทึกเหตุการณ์พร้อม timestamp

### ตรวจ Drive
ใช้ 4 method เรียงตามความน่าเชื่อถือ จนกว่าจะพบ:
1. PowerShell `Get-PhysicalDisk` — ครอบคลุม NVMe, USB, SD Card
2. PowerShell `Get-Disk`
3. `wmic diskdrive /format:list`
4. ctypes brute-force scan `PhysicalDrive0` ถึง `PhysicalDrive9`

> PowerShell ทุก call ใช้ `CREATE_NO_WINDOW` — ไม่มี console กระพริบ

### Admin Check
- ตรวจสิทธิ์ด้วย `IsUserAnAdmin()` ตอน startup
- Header แสดง ✓ สีเขียว (Admin) หรือ ✗ สีแดง (ไม่ใช่ Admin)
- ปุ่ม **START SCAN** ถูก disable อัตโนมัติถ้าไม่มีสิทธิ์
- ปุ่ม **Relaunch as Admin** โผล่เมื่อไม่มีสิทธิ์ — กดแล้ว UAC popup ขึ้นทันที

### ภาษา
สลับภาษาได้ real-time ไม่ต้อง restart ทุก label, ปุ่ม, dialog เปลี่ยนพร้อมกัน

| ปุ่ม | ภาษา |
|------|------|
| EN | English |
| TH | ภาษาไทย |
| JA | 日本語 |
| ZH | 中文简体 |

---

## วิธีอ่านผลลัพธ์

| ผลลัพธ์ | ความหมาย |
|---------|----------|
| `Scan complete — No bad sectors found` | ดิสก์ปกติ ไม่มีปัญหา |
| `Scan complete — N bad sector(s) found` | พบ bad sector แต่สแกนครบ |
| `Stopped — Bad sector detected` | หยุดเมื่อพบ bad sector แรก (mode: stop on first) |
| `Permission denied` | ไม่ได้รันในฐานะ Administrator |
| `Cannot read disk size` | ดิสก์หมายเลขนั้นไม่มีอยู่จริง |

---

## โหมด Demo (non-Windows)

รันบน macOS / Linux ได้เพื่อทดสอบ UI โดยจะจำลอง:
- 3 demo drives (Samsung / WD / Seagate)
- 200 blocks พร้อม bad sector แบบสุ่ม ~4%
- ความเร็ว 80 MB/s

---

## Changelog

### v3.1
- เพิ่ม admin check ตอน startup (`IsUserAnAdmin`)
- Header badge แสดงสถานะ Admin แบบ real-time
- ปุ่ม Relaunch as Admin พร้อม UAC elevation
- `CREATE_NO_WINDOW` ทุก subprocess — ไม่มี PowerShell กระพริบ

### v3.0
- Multi-language EN / TH / JA / ZH (live switch)
- Clean code release — rename functions, extract helpers, type hints
- `scan_disk()`, `list_drives()`, `get_disk_size()` — ชื่อชัดเจน

### v2.0
- อัปเกรด Flet 0.80+ API ทั้งหมด
- PowerShell 4-method drive detection
- Confirm dialog ก่อน stop
- Timer นับเวลา real-time

---

## Roadmap

- **v4.0** — PDF Report หลัง scan เสร็จ (disk info, block map snapshot, bad sector list, timestamp)

---

## License

Kuroneko Tools — Internal Use
