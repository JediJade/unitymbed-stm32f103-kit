/**
  * @file    usbd_hid_if.c
  * @brief   USB HID class driver (keyboard + mouse) + application helpers.
  *
  * The report descriptor declares two top level collections selected by a
  * leading report ID byte:
  *
  *   Report ID 1 - keyboard : [1] modifiers, [2] reserved, [3..8] 6 keycodes
  *   Report ID 2 - mouse    : [1] buttons,   [2] X,        [3] Y, [4] wheel
  *
  * So a keyboard report is 9 bytes on the wire and a mouse report is 5 bytes,
  * both well inside the 16-byte interrupt IN endpoint.
  */

#include "usbd_hid_if.h"
#include "usbd_ctlreq.h"
#include "usb_device.h"

/* Private prototypes ------------------------------------------------------- */
static uint8_t  USBD_HID_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t  USBD_HID_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t  USBD_HID_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t  USBD_HID_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t *USBD_HID_GetFSCfgDesc(uint16_t *length);
static uint8_t *USBD_HID_GetHSCfgDesc(uint16_t *length);
static uint8_t *USBD_HID_GetOtherSpeedCfgDesc(uint16_t *length);
static uint8_t *USBD_HID_GetDeviceQualifierDesc(uint16_t *length);

/* Class interface ---------------------------------------------------------- */
USBD_ClassTypeDef USBD_HID =
{
  USBD_HID_Init,
  USBD_HID_DeInit,
  USBD_HID_Setup,
  NULL,                 /* EP0_TxSent */
  NULL,                 /* EP0_RxReady */
  USBD_HID_DataIn,
  NULL,                 /* DataOut */
  NULL,                 /* SOF */
  NULL,                 /* IsoINIncomplete */
  NULL,                 /* IsoOUTIncomplete */
  USBD_HID_GetHSCfgDesc,
  USBD_HID_GetFSCfgDesc,
  USBD_HID_GetOtherSpeedCfgDesc,
  USBD_HID_GetDeviceQualifierDesc,
};

/* Configuration descriptor ------------------------------------------------- */
__ALIGN_BEGIN static uint8_t USBD_HID_CfgDesc[USB_HID_CONFIG_DESC_SIZ] __ALIGN_END =
{
  /******************** Configuration descriptor ********************/
  0x09,                         /* bLength */
  USB_DESC_TYPE_CONFIGURATION,  /* bDescriptorType */
  USB_HID_CONFIG_DESC_SIZ,      /* wTotalLength */
  0x00,
  0x01,                         /* bNumInterfaces */
  0x01,                         /* bConfigurationValue */
  0x00,                         /* iConfiguration */
#if (USBD_SELF_POWERED == 1U)
  0xE0,                         /* bmAttributes: self powered + remote wakeup */
#else
  0xA0,                         /* bmAttributes: bus powered + remote wakeup */
#endif /* USBD_SELF_POWERED */
  USBD_MAX_POWER,               /* bMaxPower (x2 mA) */

  /******************** HID interface descriptor ********************/
  0x09,                         /* bLength */
  USB_DESC_TYPE_INTERFACE,      /* bDescriptorType */
  0x00,                         /* bInterfaceNumber */
  0x00,                         /* bAlternateSetting */
  0x01,                         /* bNumEndpoints */
  0x03,                         /* bInterfaceClass: HID */
  0x00,                         /* bInterfaceSubClass: 0 = no boot protocol */
  0x00,                         /* bInterfaceProtocol: 0 = none (report IDs used) */
  0x00,                         /* iInterface */

  /******************** HID class descriptor ********************/
  0x09,                         /* bLength */
  HID_DESCRIPTOR_TYPE,          /* bDescriptorType: HID */
  0x11,                         /* bcdHID: 1.11 */
  0x01,
  0x00,                         /* bCountryCode */
  0x01,                         /* bNumDescriptors */
  HID_REPORT_DESC,              /* bDescriptorType: report */
  LOBYTE(HID_REPORT_DESC_SIZE), /* wItemLength */
  HIBYTE(HID_REPORT_DESC_SIZE),

  /******************** Interrupt IN endpoint descriptor ********************/
  0x07,                         /* bLength */
  USB_DESC_TYPE_ENDPOINT,       /* bDescriptorType */
  HID_EPIN_ADDR,                /* bEndpointAddress */
  0x03,                         /* bmAttributes: interrupt */
  HID_EPIN_SIZE,                /* wMaxPacketSize */
  0x00,
  HID_FS_BINTERVAL,             /* bInterval */
};

/* Stand-alone HID descriptor, returned for a GET_DESCRIPTOR(HID) request */
__ALIGN_BEGIN static uint8_t USBD_HID_Desc[USB_HID_DESC_SIZ] __ALIGN_END =
{
  0x09,
  HID_DESCRIPTOR_TYPE,
  0x11,
  0x01,
  0x00,
  0x01,
  HID_REPORT_DESC,
  LOBYTE(HID_REPORT_DESC_SIZE),
  HIBYTE(HID_REPORT_DESC_SIZE),
};

__ALIGN_BEGIN static uint8_t USBD_HID_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
{
  USB_LEN_DEV_QUALIFIER_DESC,
  USB_DESC_TYPE_DEVICE_QUALIFIER,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x40,
  0x01,
  0x00,
};

/* Report descriptor -------------------------------------------------------- */
__ALIGN_BEGIN static uint8_t HID_ReportDesc[HID_REPORT_DESC_SIZE] __ALIGN_END =
{
  /* ---------------- Keyboard, report ID 1 ---------------- */
  0x05, 0x01,       /* Usage Page (Generic Desktop)          */
  0x09, 0x06,       /* Usage (Keyboard)                      */
  0xA1, 0x01,       /* Collection (Application)              */
  0x85, HID_REPORT_ID_KEYBOARD, /* Report ID (1)             */
  0x05, 0x07,       /*   Usage Page (Keyboard/Keypad)        */
  0x19, 0xE0,       /*   Usage Minimum (Left Control)        */
  0x29, 0xE7,       /*   Usage Maximum (Right GUI)           */
  0x15, 0x00,       /*   Logical Minimum (0)                 */
  0x25, 0x01,       /*   Logical Maximum (1)                 */
  0x75, 0x01,       /*   Report Size (1)                     */
  0x95, 0x08,       /*   Report Count (8)                    */
  0x81, 0x02,       /*   Input (Data,Var,Abs) - modifiers    */
  0x75, 0x08,       /*   Report Size (8)                     */
  0x95, 0x01,       /*   Report Count (1)                    */
  0x81, 0x01,       /*   Input (Const) - reserved byte       */
  0x15, 0x00,       /*   Logical Minimum (0)                 */
  0x25, 0x65,       /*   Logical Maximum (101)               */
  0x05, 0x07,       /*   Usage Page (Keyboard/Keypad)        */
  0x19, 0x00,       /*   Usage Minimum (0)                   */
  0x29, 0x65,       /*   Usage Maximum (101)                 */
  0x75, 0x08,       /*   Report Size (8)                     */
  0x95, 0x06,       /*   Report Count (6)                    */
  0x81, 0x00,       /*   Input (Data,Array) - 6 keycodes     */
  0xC0,             /* End Collection                        */

  /* ---------------- Mouse, report ID 2 ---------------- */
  0x05, 0x01,       /* Usage Page (Generic Desktop)          */
  0x09, 0x02,       /* Usage (Mouse)                         */
  0xA1, 0x01,       /* Collection (Application)              */
  0x85, HID_REPORT_ID_MOUSE, /* Report ID (2)                */
  0x09, 0x01,       /*   Usage (Pointer)                     */
  0xA1, 0x00,       /*   Collection (Physical)               */
  0x05, 0x09,       /*     Usage Page (Button)               */
  0x19, 0x01,       /*     Usage Minimum (Button 1)          */
  0x29, 0x03,       /*     Usage Maximum (Button 3)          */
  0x15, 0x00,       /*     Logical Minimum (0)               */
  0x25, 0x01,       /*     Logical Maximum (1)               */
  0x75, 0x01,       /*     Report Size (1)                   */
  0x95, 0x03,       /*     Report Count (3)                  */
  0x81, 0x02,       /*     Input (Data,Var,Abs) - buttons    */
  0x75, 0x05,       /*     Report Size (5)                   */
  0x95, 0x01,       /*     Report Count (1)                  */
  0x81, 0x01,       /*     Input (Const) - padding           */
  0x05, 0x01,       /*     Usage Page (Generic Desktop)      */
  0x09, 0x30,       /*     Usage (X)                         */
  0x09, 0x31,       /*     Usage (Y)                         */
  0x09, 0x38,       /*     Usage (Wheel)                     */
  0x15, 0x81,       /*     Logical Minimum (-127)            */
  0x25, 0x7F,       /*     Logical Maximum (127)             */
  0x75, 0x08,       /*     Report Size (8)                   */
  0x95, 0x03,       /*     Report Count (3)                  */
  0x81, 0x06,       /*     Input (Data,Var,Rel) - X,Y,wheel  */
  0xC0,             /*   End Collection                      */
  0xC0,             /* End Collection                        */
};

/* Catches a miscount in the table above at compile time. */
_Static_assert(sizeof(HID_ReportDesc) == HID_REPORT_DESC_SIZE,
               "HID_REPORT_DESC_SIZE does not match HID_ReportDesc");

/* Class callbacks ---------------------------------------------------------- */

static uint8_t USBD_HID_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  UNUSED(cfgidx);

  USBD_HID_HandleTypeDef *hhid =
    (USBD_HID_HandleTypeDef *)USBD_malloc(sizeof(USBD_HID_HandleTypeDef));

  if (hhid == NULL)
  {
    pdev->pClassDataCmsit[pdev->classId] = NULL;
    return (uint8_t)USBD_EMEM;
  }

  pdev->pClassDataCmsit[pdev->classId] = (void *)hhid;
  pdev->pClassData = pdev->pClassDataCmsit[pdev->classId];

  pdev->ep_in[HID_EPIN_ADDR & 0xFU].bInterval = HID_FS_BINTERVAL;

  (void)USBD_LL_OpenEP(pdev, HID_EPIN_ADDR, USBD_EP_TYPE_INTR, HID_EPIN_SIZE);
  pdev->ep_in[HID_EPIN_ADDR & 0xFU].is_used = 1U;

  hhid->Protocol   = 0U;
  hhid->IdleState  = 0U;
  hhid->AltSetting = 0U;
  hhid->state      = USBD_HID_IDLE;

  return (uint8_t)USBD_OK;
}

static uint8_t USBD_HID_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  UNUSED(cfgidx);

  (void)USBD_LL_CloseEP(pdev, HID_EPIN_ADDR);
  pdev->ep_in[HID_EPIN_ADDR & 0xFU].is_used   = 0U;
  pdev->ep_in[HID_EPIN_ADDR & 0xFU].bInterval = 0U;

  if (pdev->pClassDataCmsit[pdev->classId] != NULL)
  {
    (void)USBD_free(pdev->pClassDataCmsit[pdev->classId]);
    pdev->pClassDataCmsit[pdev->classId] = NULL;
    pdev->pClassData = NULL;
  }

  return (uint8_t)USBD_OK;
}

static uint8_t USBD_HID_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
  USBD_HID_HandleTypeDef *hhid =
    (USBD_HID_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
  USBD_StatusTypeDef ret = USBD_OK;
  uint16_t status_info = 0U;
  uint16_t len;
  uint8_t *pbuf;

  if (hhid == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  switch (req->bmRequest & USB_REQ_TYPE_MASK)
  {
    case USB_REQ_TYPE_CLASS:
      switch (req->bRequest)
      {
        case USBD_HID_REQ_SET_PROTOCOL:
          hhid->Protocol = (uint8_t)(req->wValue);
          break;

        case USBD_HID_REQ_GET_PROTOCOL:
          (void)USBD_CtlSendData(pdev, (uint8_t *)&hhid->Protocol, 1U);
          break;

        case USBD_HID_REQ_SET_IDLE:
          hhid->IdleState = (uint8_t)(req->wValue >> 8);
          break;

        case USBD_HID_REQ_GET_IDLE:
          (void)USBD_CtlSendData(pdev, (uint8_t *)&hhid->IdleState, 1U);
          break;

        default:
          USBD_CtlError(pdev, req);
          ret = USBD_FAIL;
          break;
      }
      break;

    case USB_REQ_TYPE_STANDARD:
      switch (req->bRequest)
      {
        case USB_REQ_GET_STATUS:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            (void)USBD_CtlSendData(pdev, (uint8_t *)&status_info, 2U);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_GET_DESCRIPTOR:
          if ((req->wValue >> 8) == HID_REPORT_DESC)
          {
            len  = MIN(HID_REPORT_DESC_SIZE, req->wLength);
            pbuf = HID_ReportDesc;
          }
          else if ((req->wValue >> 8) == HID_DESCRIPTOR_TYPE)
          {
            len  = MIN(USB_HID_DESC_SIZ, req->wLength);
            pbuf = USBD_HID_Desc;
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
            break;
          }
          (void)USBD_CtlSendData(pdev, pbuf, len);
          break;

        case USB_REQ_GET_INTERFACE:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            (void)USBD_CtlSendData(pdev, (uint8_t *)&hhid->AltSetting, 1U);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_SET_INTERFACE:
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            hhid->AltSetting = (uint8_t)(req->wValue);
          }
          else
          {
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
          }
          break;

        case USB_REQ_CLEAR_FEATURE:
          break;

        default:
          USBD_CtlError(pdev, req);
          ret = USBD_FAIL;
          break;
      }
      break;

    default:
      USBD_CtlError(pdev, req);
      ret = USBD_FAIL;
      break;
  }

  return (uint8_t)ret;
}

static uint8_t USBD_HID_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  UNUSED(epnum);

  USBD_HID_HandleTypeDef *hhid =
    (USBD_HID_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];

  if (hhid != NULL)
  {
    /* The previous report left the FIFO, the endpoint is free again. */
    hhid->state = USBD_HID_IDLE;
  }

  return (uint8_t)USBD_OK;
}

static uint8_t *USBD_HID_GetFSCfgDesc(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_HID_CfgDesc);
  return USBD_HID_CfgDesc;
}

static uint8_t *USBD_HID_GetHSCfgDesc(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_HID_CfgDesc);
  return USBD_HID_CfgDesc;
}

static uint8_t *USBD_HID_GetOtherSpeedCfgDesc(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_HID_CfgDesc);
  return USBD_HID_CfgDesc;
}

static uint8_t *USBD_HID_GetDeviceQualifierDesc(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_HID_DeviceQualifierDesc);
  return USBD_HID_DeviceQualifierDesc;
}

/* Class level API ---------------------------------------------------------- */

uint8_t USBD_HID_SendReport(USBD_HandleTypeDef *pdev, uint8_t *report, uint16_t len)
{
  USBD_HID_HandleTypeDef *hhid =
    (USBD_HID_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];

  if (hhid == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  if (pdev->dev_state != USBD_STATE_CONFIGURED)
  {
    return (uint8_t)USBD_FAIL;
  }

  if (hhid->state != USBD_HID_IDLE)
  {
    return (uint8_t)USBD_BUSY;
  }

  hhid->state = USBD_HID_BUSY;
  (void)USBD_LL_Transmit(pdev, HID_EPIN_ADDR, report, len);

  return (uint8_t)USBD_OK;
}

uint32_t USBD_HID_GetPollingInterval(USBD_HandleTypeDef *pdev)
{
  UNUSED(pdev);
  /* Full speed: bInterval is already expressed in milliseconds. */
  return (uint32_t)HID_FS_BINTERVAL;
}

/* Application level API ---------------------------------------------------- */

uint8_t HID_Ready(void)
{
  USBD_HID_HandleTypeDef *hhid =
    (USBD_HID_HandleTypeDef *)hUsbDeviceFS.pClassDataCmsit[hUsbDeviceFS.classId];

  if ((hhid == NULL) || (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED))
  {
    return 0U;
  }

  return (hhid->state == USBD_HID_IDLE) ? 1U : 0U;
}

/* Waits for the IN endpoint to drain, so back to back reports are not lost. */
static uint8_t HID_WaitReady(uint32_t timeout_ms)
{
  uint32_t start = HAL_GetTick();

  while (HID_Ready() == 0U)
  {
    if ((HAL_GetTick() - start) > timeout_ms)
    {
      return 0U;
    }
  }

  return 1U;
}

uint8_t HID_Mouse_Send(int8_t x, int8_t y, int8_t wheel, uint8_t buttons)
{
  uint8_t report[5];

  report[0] = HID_REPORT_ID_MOUSE;
  report[1] = buttons;
  report[2] = (uint8_t)x;
  report[3] = (uint8_t)y;
  report[4] = (uint8_t)wheel;

  return USBD_HID_SendReport(&hUsbDeviceFS, report, sizeof(report));
}

uint8_t HID_Mouse_Click(uint8_t buttons)
{
  if (HID_WaitReady(50U) == 0U)
  {
    return (uint8_t)USBD_BUSY;
  }
  (void)HID_Mouse_Send(0, 0, 0, buttons);

  /* Hold long enough for the host to see the press, then release. */
  HAL_Delay(2U * HID_FS_BINTERVAL);

  if (HID_WaitReady(50U) == 0U)
  {
    return (uint8_t)USBD_BUSY;
  }
  return HID_Mouse_Send(0, 0, 0, 0U);
}

uint8_t HID_Keyboard_Send(uint8_t modifiers, const uint8_t keys[6])
{
  uint8_t report[9];
  uint8_t i;

  report[0] = HID_REPORT_ID_KEYBOARD;
  report[1] = modifiers;
  report[2] = 0U;                 /* reserved */

  for (i = 0U; i < 6U; i++)
  {
    report[3U + i] = (keys != NULL) ? keys[i] : 0U;
  }

  return USBD_HID_SendReport(&hUsbDeviceFS, report, sizeof(report));
}

uint8_t HID_Keyboard_Release(void)
{
  return HID_Keyboard_Send(0U, NULL);
}

uint8_t HID_Keyboard_Tap(uint8_t modifiers, uint8_t key)
{
  uint8_t keys[6] = {0U, 0U, 0U, 0U, 0U, 0U};

  keys[0] = key;

  if (HID_WaitReady(50U) == 0U)
  {
    return (uint8_t)USBD_BUSY;
  }
  (void)HID_Keyboard_Send(modifiers, keys);

  HAL_Delay(2U * HID_FS_BINTERVAL);

  if (HID_WaitReady(50U) == 0U)
  {
    return (uint8_t)USBD_BUSY;
  }
  return HID_Keyboard_Release();
}

/* Maps 7-bit ASCII to a US layout usage ID, 0 when the character has none. */
static uint8_t HID_AsciiToKey(char c, uint8_t *modifiers)
{
  *modifiers = 0U;

  if ((c >= 'a') && (c <= 'z'))
  {
    return (uint8_t)(HID_KEY_A + (c - 'a'));
  }

  if ((c >= 'A') && (c <= 'Z'))
  {
    *modifiers = HID_MOD_LSHIFT;
    return (uint8_t)(HID_KEY_A + (c - 'A'));
  }

  if ((c >= '1') && (c <= '9'))
  {
    return (uint8_t)(HID_KEY_1 + (c - '1'));
  }

  switch (c)
  {
    case '0':  return HID_KEY_0;
    case ' ':  return HID_KEY_SPACE;
    case '\n': return HID_KEY_ENTER;
    case '\r': return HID_KEY_ENTER;
    case '\t': return HID_KEY_TAB;
    case '-':  return 0x2DU;
    case '=':  return 0x2EU;
    case '[':  return 0x2FU;
    case ']':  return 0x30U;
    case '\\': return 0x31U;
    case ';':  return 0x33U;
    case '\'': return 0x34U;
    case '`':  return 0x35U;
    case ',':  return 0x36U;
    case '.':  return 0x37U;
    case '/':  return 0x38U;
    default:   break;
  }

  /* Characters that need shift on a US layout. */
  *modifiers = HID_MOD_LSHIFT;

  switch (c)
  {
    case '!':  return HID_KEY_1;
    case '@':  return (uint8_t)(HID_KEY_1 + 1);
    case '#':  return (uint8_t)(HID_KEY_1 + 2);
    case '$':  return (uint8_t)(HID_KEY_1 + 3);
    case '%':  return (uint8_t)(HID_KEY_1 + 4);
    case '^':  return (uint8_t)(HID_KEY_1 + 5);
    case '&':  return (uint8_t)(HID_KEY_1 + 6);
    case '*':  return (uint8_t)(HID_KEY_1 + 7);
    case '(':  return (uint8_t)(HID_KEY_1 + 8);
    case ')':  return HID_KEY_0;
    case '_':  return 0x2DU;
    case '+':  return 0x2EU;
    case '{':  return 0x2FU;
    case '}':  return 0x30U;
    case '|':  return 0x31U;
    case ':':  return 0x33U;
    case '"':  return 0x34U;
    case '~':  return 0x35U;
    case '<':  return 0x36U;
    case '>':  return 0x37U;
    case '?':  return 0x38U;
    default:   break;
  }

  *modifiers = 0U;
  return HID_KEY_NONE;
}

void HID_Keyboard_TypeString(const char *str)
{
  uint8_t modifiers;
  uint8_t key;
  uint8_t prev_key = HID_KEY_NONE;

  if (str == NULL)
  {
    return;
  }

  while (*str != '\0')
  {
    key = HID_AsciiToKey(*str, &modifiers);

    if (key != HID_KEY_NONE)
    {
      /* Repeating the same key needs an explicit release in between,
         otherwise the host sees one long press instead of two taps. */
      if (key == prev_key)
      {
        if (HID_WaitReady(50U) != 0U)
        {
          (void)HID_Keyboard_Release();
          HAL_Delay(HID_FS_BINTERVAL);
        }
      }

      (void)HID_Keyboard_Tap(modifiers, key);
      prev_key = key;
    }

    str++;
  }

  if (HID_WaitReady(50U) != 0U)
  {
    (void)HID_Keyboard_Release();
  }
}
