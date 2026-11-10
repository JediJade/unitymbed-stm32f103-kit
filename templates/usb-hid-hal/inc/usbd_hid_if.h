/**
  * @file    usbd_hid_if.h
  * @brief   USB HID (keyboard + mouse) class driver and application API.
  *
  * Self-contained HID class driver written against the STM32 USB device
  * library in stm32-mw-usb-device-master/Core.
  *
  * NOTE: do NOT also compile stm32-mw-usb-device-master/Class/HID/Src/usbd_hid.c
  *       - it exports the same USBD_HID symbol (mouse only, 4-byte reports).
  */
#ifndef __USBD_HID_IF_H__
#define __USBD_HID_IF_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "usbd_ioreq.h"

/* Endpoint / descriptor sizes ---------------------------------------------- */
#define HID_EPIN_ADDR                 0x81U
#define HID_EPIN_SIZE                 0x10U  /* 16 B: fits the 9-byte keyboard report */
#define HID_FS_BINTERVAL              0x0AU  /* 10 ms polling interval */

#define USB_HID_CONFIG_DESC_SIZ       34U
#define USB_HID_DESC_SIZ              9U
#define HID_REPORT_DESC_SIZE          101U

#define HID_DESCRIPTOR_TYPE           0x21U
#define HID_REPORT_DESC               0x22U

/* HID class requests ------------------------------------------------------- */
#define USBD_HID_REQ_SET_PROTOCOL     0x0BU
#define USBD_HID_REQ_GET_PROTOCOL     0x03U
#define USBD_HID_REQ_SET_IDLE         0x0AU
#define USBD_HID_REQ_GET_IDLE         0x02U
#define USBD_HID_REQ_SET_REPORT       0x09U
#define USBD_HID_REQ_GET_REPORT       0x01U

/* Report IDs (must match the report descriptor in usbd_hid_if.c) ----------- */
#define HID_REPORT_ID_KEYBOARD        0x01U
#define HID_REPORT_ID_MOUSE           0x02U

/* Keyboard modifier bits --------------------------------------------------- */
#define HID_MOD_LCTRL                 0x01U
#define HID_MOD_LSHIFT                0x02U
#define HID_MOD_LALT                  0x04U
#define HID_MOD_LGUI                  0x08U
#define HID_MOD_RCTRL                 0x10U
#define HID_MOD_RSHIFT                0x20U
#define HID_MOD_RALT                  0x40U
#define HID_MOD_RGUI                  0x80U

/* A few HID usage IDs (Usage Page 0x07). Full list: HID Usage Tables ch. 10 */
#define HID_KEY_NONE                  0x00U
#define HID_KEY_A                     0x04U
#define HID_KEY_B                     0x05U
#define HID_KEY_C                     0x06U
#define HID_KEY_D                     0x07U
#define HID_KEY_E                     0x08U
#define HID_KEY_H                     0x0BU
#define HID_KEY_L                     0x0FU
#define HID_KEY_O                     0x12U
#define HID_KEY_Z                     0x1DU
#define HID_KEY_1                     0x1EU
#define HID_KEY_0                     0x27U
#define HID_KEY_ENTER                 0x28U
#define HID_KEY_ESC                   0x29U
#define HID_KEY_BACKSPACE             0x2AU
#define HID_KEY_TAB                   0x2BU
#define HID_KEY_SPACE                 0x2CU
#define HID_KEY_CAPSLOCK              0x39U
#define HID_KEY_F1                    0x3AU
#define HID_KEY_F4                    0x3DU
#define HID_KEY_RIGHT                 0x4FU
#define HID_KEY_LEFT                  0x50U
#define HID_KEY_DOWN                  0x51U
#define HID_KEY_UP                    0x52U

/* Mouse button bits -------------------------------------------------------- */
#define HID_MOUSE_BTN_LEFT            0x01U
#define HID_MOUSE_BTN_RIGHT           0x02U
#define HID_MOUSE_BTN_MIDDLE          0x04U

/* Class driver state ------------------------------------------------------- */
typedef enum
{
  USBD_HID_IDLE = 0,
  USBD_HID_BUSY,
} USBD_HID_StateTypeDef;

typedef struct
{
  uint32_t              Protocol;
  uint32_t              IdleState;
  uint32_t              AltSetting;
  USBD_HID_StateTypeDef state;
} USBD_HID_HandleTypeDef;

/* HID descriptor, HID 1.11 section 6.2.1 */
typedef struct
{
  uint8_t  bLength;
  uint8_t  bDescriptorType;
  uint16_t bcdHID;
  uint8_t  bCountryCode;
  uint8_t  bNumDescriptors;
  uint8_t  bHIDDescriptorType;
  uint16_t wItemLength;
} __PACKED USBD_HIDDescTypeDef;

extern USBD_ClassTypeDef USBD_HID;
#define USBD_HID_CLASS &USBD_HID

/* Class level API ---------------------------------------------------------- */
uint8_t USBD_HID_SendReport(USBD_HandleTypeDef *pdev, uint8_t *report, uint16_t len);
uint32_t USBD_HID_GetPollingInterval(USBD_HandleTypeDef *pdev);

/* Application level API (uses hUsbDeviceFS) -------------------------------- */

/* 1 when the host has configured the device and the IN endpoint is free. */
uint8_t HID_Ready(void);

/* Mouse: x/y/wheel are relative steps (-127..127), buttons is a bit mask. */
uint8_t HID_Mouse_Send(int8_t x, int8_t y, int8_t wheel, uint8_t buttons);
uint8_t HID_Mouse_Click(uint8_t buttons);

/* Keyboard: keys[] holds up to 6 simultaneous usage IDs, 0 = unused. */
uint8_t HID_Keyboard_Send(uint8_t modifiers, const uint8_t keys[6]);
uint8_t HID_Keyboard_Release(void);
uint8_t HID_Keyboard_Tap(uint8_t modifiers, uint8_t key);

/* Types a 7-bit ASCII string (US layout). Blocks ~2x HID_FS_BINTERVAL/char. */
void HID_Keyboard_TypeString(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* __USBD_HID_IF_H__ */
