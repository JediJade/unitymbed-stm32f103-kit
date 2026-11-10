# UnityMbed STM32F103 kit (ภาษาไทย)

ชุดโปรเจกต์ตั้งต้นสำหรับ STM32F103 (บอร์ด "Blue Pill") ที่ใช้ร่วมกับ ST-Link
จัดโครงสร้างตามที่ UnityMbed IDE ต้องการ คือมี `Makefile`, `linker.ld`,
`openocd.cfg` และ `unitymbed.json` ทุก template ยัง build ด้วย `make` ธรรมดาใน
Git Bash หรือ Linux ได้ด้วย

English: [README.md](README.md)

## Template ที่มี

| template | ทำอะไร | ขนาด flash | สร้างบน | สถานะ |
| --- | --- | --- | --- | --- |
| [`blink-bare`](templates/blink-bare) | กระพริบ LED PC13, clock 72 MHz, SysTick | 0.5 KB | register ตรง | build ผ่าน, ยังไม่ได้แฟลชลงบอร์ด |
| [`usb-cdc-bare`](templates/usb-cdc-bare) | USB virtual COM port, ส่งกลับทุกตัวอักษร (echo) | 2.7 KB | register ตรง | ไดรเวอร์เคยรันบนบอร์ดจริงในอีกโปรเจกต์ แต่ image นี้ยังไม่ได้แฟลช |
| [`usb-hid-bare`](templates/usb-hid-bare) | USB คีย์บอร์ด + เมาส์ | 3.2 KB | register ตรง | **ทดสอบบนบอร์ดจริงแล้ว** |
| [`usb-cdc-hal`](templates/usb-cdc-hal) | USB virtual COM port, ส่งค่า ADC จาก PA0 | 22 KB | ST HAL + USB library | **ทดสอบบนบอร์ดจริงแล้ว** |
| [`usb-hid-hal`](templates/usb-hid-hal) | USB คีย์บอร์ด + เมาส์ | 16 KB | ST HAL + USB library | **ทดสอบบนบอร์ดจริงแล้ว** |

"ทดสอบบนบอร์ดจริงแล้ว" หมายถึง template นั้น build ออกมาได้ไฟล์ `.bin`
ที่ตรงกันทุกไบต์ (เทียบด้วย sha256) กับโปรเจกต์ที่เคยรันบน Blue Pill มาแล้ว
โดยใช้ arm-none-eabi-gcc 14.2.1 ตัวเดียวกับที่มากับ UnityMbed

template แบบ bare metal ใช้แค่ header ของ CMSIS กับ STM32F103 ส่วน template
แบบ HAL มีเฉพาะไฟล์ของ ST HAL, CMSIS-device และ USB device library
ที่ต้อง compile จริง ไม่ได้ใส่ไฟล์ทั้งชุดมา

## โครงสร้าง repository

```
templates/        หนึ่งโฟลเดอร์ต่อหนึ่ง template - คัดลอกไปแล้ว build ได้เลย
common/           CMSIS header, linker script, ไฟล์ตั้งค่า OpenOCD (ชุดอ้างอิง)
drivers/windows/  ต้องลงไดรเวอร์อะไรบ้าง (มีแค่ ST-Link), วิธีต่อสาย, การแก้ปัญหา
tools/
  new-project.ps1     คัดลอก template ไปที่ ~/UnityMbed/<ชื่อ> แล้วตั้งชื่อให้
  build-all.sh        build ทุก template และตรวจ vector table ของแต่ละตัว
  serial-sniffer.ps1  อ่านข้อมูลจาก COM port (template CDC, ST-Link VCP, USB-TTL)
  hid-sniffer.ps1     ดูว่า Windows อ่าน HID descriptor ได้อย่างไร และดู report สด
  usb-sniffer.ps1     ดูอุปกรณ์ ST / STM32 ที่เสียบเข้าและถอดออก
```

## เริ่มโปรเจกต์ใหม่

1. ติดตั้ง UnityMbed IDE ซึ่งมี arm-none-eabi-gcc, make และ OpenOCD มาให้แล้ว
2. ติดตั้งไดรเวอร์ ST-Link (ทำครั้งเดียว) ดูที่ [drivers/windows](drivers/windows/README.md)
3. สร้างโปรเจกต์จาก template:

   ```powershell
   .\tools\new-project.ps1 -List
   .\tools\new-project.ps1 -Template usb-cdc-bare -Name MySensor
   ```

   คำสั่งนี้จะสร้างโฟลเดอร์ `%USERPROFILE%\UnityMbed\MySensor` และตั้งค่า
   `"name"` ใน `unitymbed.json` ให้ จะคัดลอกโฟลเดอร์ template เองด้วยมือก็ได้เช่นกัน
4. เปิดโฟลเดอร์ใน UnityMbed แล้วกด build และ flash หรือใช้ Git Bash:

   ```sh
   make PROJECT=MySensor
   make flash PROJECT=MySensor
   ```

## การต่อสาย

| ST-Link V2 | Blue Pill |
| --- | --- |
| SWDIO | DIO (PA13) |
| SWCLK | CLK (PA14) |
| GND | GND |
| 3.3V | 3.3 |

ตั้ง BOOT0 = 0 สำหรับ template USB ต้องเสียบช่อง USB ของ Blue Pill เข้ากับ
PC ด้วย เพราะ ST-Link ทำหน้าที่แค่โปรแกรมชิป

## กับดัก 2 อย่างที่ควรรู้ก่อน

**`unitymbed init` อาจทำให้ vector table หาย** คำสั่งนี้เขียนทับ `linker.ld`
ด้วย template ของ IDE ซึ่งเคยใช้ชื่อ section ว่า `.vectors` แต่โปรเจกต์ในชุดนี้วาง
vector table ไว้ใน `.isr_vector` เมื่อใช้ `--gc-sections` ตัว linker จะทิ้ง
vector table ทั้งก้อนโดยไม่เตือนอะไรเลย ผลคือ build ผ่าน แฟลชผ่าน verify ผ่าน
แต่ชิปค้างก่อนเข้า `main()` และเพราะ Blue Pill มีตัวต้านทาน pull-up ที่ขา D+
PC จึงยังเห็นอุปกรณ์ USB อยู่ อาการเลยดูเหมือนปัญหา USB ("Unknown USB Device",
code 43) ทั้งที่จริงคือชิปไม่ได้ทำงาน linker script ในชุดนี้ป้องกันไว้แล้ว
แต่หลังรัน `unitymbed init` ทุกครั้งให้ตรวจด้วย:

```sh
sh tools/build-all.sh
```

สคริปต์จะแจ้ง FAIL ถ้า image ใดไม่ได้ขึ้นต้นด้วย stack pointer ที่ชี้ไปใน RAM
และ reset vector ที่เป็นเลขคี่ (Thumb)

**Windows จำ USB descriptor ไว้ตาม VID:PID** แต่ละ template จึงใช้ PID ของตัวเอง
(0x5740, 0x5750, 0x5751, 0x5752) เพื่อไม่ให้ firmware ตัวหนึ่งไปได้ descriptor
ที่ Windows จำไว้ของอีกตัว โปรเจกต์ของคุณเองก็ควรใช้ PID แยกเช่นกัน VID 0x0483
เป็นของ ST ใช้ทดลองบนโต๊ะตัวเองได้ แต่ถ้าจะทำเป็นผลิตภัณฑ์ต้องมี VID/PID
ของตัวเอง

## ไลเซนส์

โค้ดที่เขียนขึ้นสำหรับชุดนี้ใช้ไลเซนส์ MIT ([LICENSE](LICENSE)) ส่วนไฟล์ของ Arm
และ STMicroelectronics ยังคงใช้ไลเซนส์เดิมของเจ้าของ ดูรายละเอียดใน
[THIRD_PARTY.md](THIRD_PARTY.md) ไดรเวอร์ ST-Link ไม่ได้รวมอยู่ใน repo นี้
ต้องดาวน์โหลดจาก st.com เอง
