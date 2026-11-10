/*
 * HID report helpers on top of the bare metal USB driver in usb.c.
 */
#ifndef HID_H
#define HID_H

#include <stdint.h>

#define HID_REPORT_ID_KEYBOARD   0x01U
#define HID_REPORT_ID_MOUSE      0x02U

/* Keyboard modifier bits */
#define HID_MOD_LCTRL            0x01U
#define HID_MOD_LSHIFT           0x02U
#define HID_MOD_LALT             0x04U
#define HID_MOD_LGUI             0x08U
#define HID_MOD_RCTRL            0x10U
#define HID_MOD_RSHIFT           0x20U
#define HID_MOD_RALT             0x40U
#define HID_MOD_RGUI             0x80U

/* A few usage IDs from Usage Page 0x07 */
#define HID_KEY_NONE             0x00U
#define HID_KEY_A                0x04U
#define HID_KEY_D                0x07U
#define HID_KEY_1                0x1EU
#define HID_KEY_0                0x27U
#define HID_KEY_ENTER            0x28U
#define HID_KEY_ESC              0x29U
#define HID_KEY_BACKSPACE        0x2AU
#define HID_KEY_TAB              0x2BU
#define HID_KEY_SPACE            0x2CU
#define HID_KEY_F4               0x3DU
#define HID_KEY_RIGHT            0x4FU
#define HID_KEY_LEFT             0x50U
#define HID_KEY_DOWN             0x51U
#define HID_KEY_UP               0x52U

/* Mouse button bits */
#define HID_MOUSE_BTN_LEFT       0x01U
#define HID_MOUSE_BTN_RIGHT      0x02U
#define HID_MOUSE_BTN_MIDDLE     0x04U

/* All of these return 0 on success, -1 when the endpoint is not ready. */
int  hid_mouse_send(int8_t x, int8_t y, int8_t wheel, uint8_t buttons);
int  hid_mouse_click(uint8_t buttons);

int  hid_keyboard_send(uint8_t modifiers, const uint8_t keys[6]);
int  hid_keyboard_release(void);
int  hid_keyboard_tap(uint8_t modifiers, uint8_t key);

/* Types 7-bit ASCII on a US layout. Blocks while it types. */
void hid_keyboard_type(const char *str);

#endif /* HID_H */
