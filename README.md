# BlackCat Disk Scanner

เครื่องมือสแกน HDD / SSD ระดับ sector บน Windows พร้อม UI แบบ real-time

---

## Requirements

- Windows (จำเป็นต้องรันในฐานะ Administrator สำหรับการอ่าน raw disk)
- Python 3.10+
- Flet >= 0.80

```bash
pip install flet
```

---

## Run

```bash
python disk_scan.py
```

> หากไม่ได้รันเป็น Administrator โปรแกรมจะแสดงปุ่ม **Relaunch as Admin** ให้อัตโนมัติ
> หรือเปิด **Demo Mode** เพื่อทดสอบ UI โดยไม่ต้องใช้สิทธิ์

---

## Features

### การสแกน
| โหมด | Block Size | เหมาะกับ |
|------|-----------|---------|
| Quick | 128 MB / block | ตรวจสอบเบื้องต้นรวดเร็ว |
| Full | 512 KB / block | ตรวจสอบละเอียด ครอบคลุมทุก sector |

- ตรวจจับ **bad sector** จากการ read error ระดับ OS
- เลือกพฤติกรรมเมื่อพบ bad sector: **หยุดทันที** หรือ **สแกนต่อจนจบ**
- แสดง **Serial Number (S/N)** ของ drive ที่เลือก

### UI
- **Disk Map** — grid แสดงสถานะแต่ละ block (เทา = ยังไม่สแกน, เขียว = OK, แดง = Bad)
- **Progress bar** พร้อม MB scanned / total, ความเร็ว MB/s, จำนวน bad sector
- **Event Log** บันทึกเหตุการณ์พร้อม timestamp
- **Demo Mode** — จำลองการสแกนแบบสุ่มโดยไม่อ่าน disk จริง

### ภาษา
- รองรับ **ภาษาไทย** และ **ภาษาอังกฤษ** สลับได้ทันทีโดยไม่ต้องรีสตาร์ท

---

## Drive Detection

ตรวจจับ drive อัตโนมัติผ่าน 3 วิธีเรียงลำดับ:
1. `Get-PhysicalDisk` (PowerShell)
2. `Get-Disk` (PowerShell)
3. `wmic diskdrive` (fallback)

---

## Roadmap

- [ ] v4.0 — PDF report generation หลังสแกนเสร็จ

---

## Changelog

### v3.2.0
- เพิ่ม **Dark / Light Theme** สลับได้ทันทีผ่านปุ่มในแถบ header
- Light mode: พื้นหลังขาว, ข้อความเข้ม, border เทาอ่อน
- Dark mode: ธีมเดิม พื้นหลังดำ

### v3.1.0
- Initial release
- HDD/SSD sector-level read scan
- EN / TH language support
- Quick / Full scan mode
- Disk Map grid, Progress bar, Event Log
- Demo Mode
- Serial Number (S/N) display

---

## License

Internal tool — BlackCat Project
