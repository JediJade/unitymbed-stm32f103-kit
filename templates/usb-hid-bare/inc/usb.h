/*
 * Minimal USB 2.0 full-speed device for the STM32F103 USB peripheral,
 * written straight on the registers: no HAL, no ST USB device library.
 *
 * It implements exactly what a HID keyboard+mouse needs:
 *   - EP0 control endpoint with a chunked IN data stage
 *   - the standard requests a Windows/Linux host asks for during enumeration
 *   - the HID class requests (GET/SET_IDLE, GET/SET_PROTOCOL)
 *   - one 16-byte interrupt IN endpoint (EP1) for reports
 */
#ifndef USB_H
#define USB_H

#include <stdint.h>

/* Report sizes on the wire, including the leading report ID byte */
#define USB_HID_KEYBOARD_REPORT_LEN   9U
#define USB_HID_MOUSE_REPORT_LEN      5U

/* Interrupt IN endpoint: 16 bytes, polled every 10 ms */
#define USB_HID_EP_SIZE               16U
#define USB_HID_EP_INTERVAL_MS        10U

void usb_init(void);

/* 1 once the host has sent SET_CONFIGURATION */
int usb_configured(void);

/* 1 when the device is configured and EP1 IN is free */
int usb_hid_ready(void);

/* Queues one report on EP1 IN. Returns 0 on success, -1 if not ready. */
int usb_hid_send(const uint8_t *report, uint16_t len);

#endif /* USB_H */
