#include "usb_device.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_hid_if.h"

USBD_HandleTypeDef hUsbDeviceFS;

void MX_USB_DEVICE_Init(void)
{
  /* 1. Init USB Device Library */
  if (USBD_Init(&hUsbDeviceFS, &FS_Desc, 0) != USBD_OK) {
    Error_Handler();
  }

  /* 2. Register the HID Class (keyboard + mouse, see usbd_hid_if.c).
        HID needs no interface callbacks: reports are pushed by the
        application through HID_Mouse_Send() / HID_Keyboard_*(). */
  if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_HID) != USBD_OK) {
    Error_Handler();
  }

  /* 3. Start the USB Device */
  if (USBD_Start(&hUsbDeviceFS) != USBD_OK) {
    Error_Handler();
  }
}
