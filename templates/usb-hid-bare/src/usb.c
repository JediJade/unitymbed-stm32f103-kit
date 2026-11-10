/*
 * STM32F103 USB device, register level.
 *
 * Things worth knowing before reading this file:
 *
 * 1. PMA (packet memory area) is 512 bytes of dedicated SRAM. The CPU sees
 *    each 16-bit word in a 32-bit slot, so a buffer at PMA offset `o` is read
 *    and written at 0x40006000 + o*2. Only 16-bit accesses.
 *
 * 2. The buffer description table (BTABLE) lives in that same memory. For
 *    endpoint n it holds ADDR_TX, COUNT_TX, ADDR_RX, COUNT_RX as four 16-bit
 *    words at BTABLE + n*8. COUNT_RX is not a byte count: it encodes the
 *    buffer size as a block count (see rx_blocks()).
 *
 * 3. USB_EPnR is the trap. STAT_RX, STAT_TX, DTOG_RX and DTOG_TX are
 *    toggle-write: the hardware XORs whatever you write into them, so to set
 *    a value you must write current ^ wanted. CTR_RX and CTR_TX are rc_w0:
 *    writing 0 clears them, so every other write must put 1 there or you lose
 *    an event - and that includes the write that clears the *other* CTR bit
 *    (see ep_clear_ctr_rx()). Both rules are handled by the ep_set_*_status()
 *    and ep_clear_ctr_*() helpers; never assign to EPR() directly except when
 *    configuring a fresh endpoint.
 */

#include "stm32f103xb.h"
#include "usb.h"

/* ------------------------------------------------------------------ PMA --- */

#define PMA_BASE          0x40006000UL
#define PMA16(off)        (*(volatile uint16_t *)(PMA_BASE + (uint32_t)(off) * 2U))

#define BT_ADDR_TX(ep)    PMA16((ep) * 8U + 0U)
#define BT_COUNT_TX(ep)   PMA16((ep) * 8U + 2U)
#define BT_ADDR_RX(ep)    PMA16((ep) * 8U + 4U)
#define BT_COUNT_RX(ep)   PMA16((ep) * 8U + 6U)

/* PMA layout: BTABLE 0x00..0x3F, then the endpoint buffers */
#define EP0_TX_OFF        0x40U
#define EP0_RX_OFF        0x80U
#define EP1_TX_OFF        0xC0U
#define EP0_SIZE          64U

/* ------------------------------------------------------------- EPnR ------- */

#define EPR(ep)           (*(volatile uint16_t *)(USB_BASE + 4U * (uint32_t)(ep)))

/* Bits that are plain rw or rc_w0 - preserved on every write */
#define EP_KEEP_MASK      (USB_EP0R_CTR_RX | USB_EP0R_SETUP | USB_EP0R_EP_TYPE | \
                           USB_EP0R_EP_KIND | USB_EP0R_CTR_TX | USB_EP0R_EA)

#define EP_TX_DIS         0x0000U
#define EP_TX_STALL       USB_EP0R_STAT_TX_0
#define EP_TX_NAK         USB_EP0R_STAT_TX_1
#define EP_TX_VALID       USB_EP0R_STAT_TX

#define EP_RX_DIS         0x0000U
#define EP_RX_STALL       USB_EP0R_STAT_RX_0
#define EP_RX_NAK         USB_EP0R_STAT_RX_1
#define EP_RX_VALID       USB_EP0R_STAT_RX

#define EP_TYPE_CONTROL   USB_EP0R_EP_TYPE_0
#define EP_TYPE_INTERRUPT USB_EP0R_EP_TYPE

static void ep_set_tx_status(uint8_t ep, uint16_t stat)
{
  uint16_t v = (uint16_t)((EPR(ep) & (EP_KEEP_MASK | USB_EP0R_STAT_TX)) ^ stat);
  EPR(ep) = (uint16_t)(v | USB_EP0R_CTR_RX | USB_EP0R_CTR_TX);
}

static void ep_set_rx_status(uint8_t ep, uint16_t stat)
{
  uint16_t v = (uint16_t)((EPR(ep) & (EP_KEEP_MASK | USB_EP0R_STAT_RX)) ^ stat);
  EPR(ep) = (uint16_t)(v | USB_EP0R_CTR_RX | USB_EP0R_CTR_TX);
}

/*
 * Clear one CTR bit and leave the other alone - which means writing 1 to the
 * other, not writing back what was read.
 *
 * Both are rc_w0: writing 0 clears, writing 1 does nothing. These two used
 * to write the other bit back as it was read. If that read said 0 and the
 * transfer in the other direction finished before the write landed, the
 * write cleared a CTR bit nobody had seen yet, and the handler for that
 * transfer never ran.
 *
 * Here only EP0 carries traffic both ways (EP1 is IN only, its receiver is
 * disabled), so the exposure is a control transfer: an IN completion being
 * cleared just as the host's next OUT or SETUP on EP0 completes. That was
 * never seen to fail in this project. It did fail in the CDC driver derived
 * from this file, where a bulk OUT packet arriving during an IN completion
 * left the endpoint at NAK for good (EP1R read 0x2021 with nothing
 * received) - same two functions, so the same fix is applied here.
 */
static void ep_clear_ctr_rx(uint8_t ep)
{
  EPR(ep) = (uint16_t)((EPR(ep) & EP_KEEP_MASK & ~USB_EP0R_CTR_RX) | USB_EP0R_CTR_TX);
}

static void ep_clear_ctr_tx(uint8_t ep)
{
  EPR(ep) = (uint16_t)((EPR(ep) & EP_KEEP_MASK & ~USB_EP0R_CTR_TX) | USB_EP0R_CTR_RX);
}

/* COUNT_RX encoding: >62 bytes counts 32-byte blocks, otherwise 2-byte ones */
static uint16_t rx_blocks(uint16_t size)
{
  if (size > 62U) {
    return (uint16_t)(USB_COUNT0_RX_BLSIZE | (((size / 32U) - 1U) << 10));
  }
  return (uint16_t)(((size + 1U) / 2U) << 10);
}

static void pma_write(uint16_t off, const uint8_t *src, uint16_t len)
{
  uint16_t i = 0U;

  while ((uint16_t)(i + 1U) < len) {
    PMA16(off + i) = (uint16_t)(src[i] | ((uint16_t)src[i + 1U] << 8));
    i = (uint16_t)(i + 2U);
  }
  if (i < len) {
    PMA16(off + i) = src[i];
  }
}

static void pma_read(uint16_t off, uint8_t *dst, uint16_t len)
{
  uint16_t i;

  for (i = 0U; i < len; i = (uint16_t)(i + 2U)) {
    uint16_t w = PMA16(off + i);
    dst[i] = (uint8_t)w;
    if ((uint16_t)(i + 1U) < len) {
      dst[i + 1U] = (uint8_t)(w >> 8);
    }
  }
}

/* ------------------------------------------------------ descriptors ------- */

#define USB_VID           0x0483U
#define USB_PID           0x5751U   /* 0x5750 is the HAL build - keep them apart
                                       so Windows does not reuse cached
                                       descriptors for a different firmware */

#define DESC_DEVICE       0x01U
#define DESC_CONFIG       0x02U
#define DESC_STRING       0x03U
#define DESC_HID          0x21U
#define DESC_HID_REPORT   0x22U

#define HID_REPORT_DESC_LEN  101U

static const uint8_t device_desc[18] = {
  18,                     /* bLength */
  DESC_DEVICE,            /* bDescriptorType */
  0x00, 0x02,             /* bcdUSB 2.00 */
  0x00,                   /* bDeviceClass: 0, the HID class is on the interface */
  0x00,                   /* bDeviceSubClass */
  0x00,                   /* bDeviceProtocol */
  EP0_SIZE,               /* bMaxPacketSize0 */
  (uint8_t)USB_VID, (uint8_t)(USB_VID >> 8),
  (uint8_t)USB_PID, (uint8_t)(USB_PID >> 8),
  0x00, 0x02,             /* bcdDevice 2.00 */
  0x01,                   /* iManufacturer */
  0x02,                   /* iProduct */
  0x03,                   /* iSerialNumber */
  0x01,                   /* bNumConfigurations */
};

static const uint8_t config_desc[34] = {
  /* configuration */
  0x09, DESC_CONFIG,
  34, 0x00,               /* wTotalLength */
  0x01,                   /* bNumInterfaces */
  0x01,                   /* bConfigurationValue */
  0x00,                   /* iConfiguration */
  0xA0,                   /* bmAttributes: bus powered, remote wakeup */
  0x32,                   /* bMaxPower: 100 mA */

  /* interface: HID, no boot protocol (report IDs are used) */
  0x09, 0x04,
  0x00,                   /* bInterfaceNumber */
  0x00,                   /* bAlternateSetting */
  0x01,                   /* bNumEndpoints */
  0x03,                   /* bInterfaceClass: HID */
  0x00,                   /* bInterfaceSubClass */
  0x00,                   /* bInterfaceProtocol */
  0x00,                   /* iInterface */

  /* HID class descriptor */
  0x09, DESC_HID,
  0x11, 0x01,             /* bcdHID 1.11 */
  0x00,                   /* bCountryCode */
  0x01,                   /* bNumDescriptors */
  DESC_HID_REPORT,
  HID_REPORT_DESC_LEN, 0x00,

  /* endpoint 1 IN, interrupt, 16 bytes, 10 ms */
  0x07, 0x05,
  0x81,                   /* bEndpointAddress */
  0x03,                   /* bmAttributes: interrupt */
  USB_HID_EP_SIZE, 0x00,  /* wMaxPacketSize */
  USB_HID_EP_INTERVAL_MS, /* bInterval */
};

/* Two top level collections, selected by a leading report ID byte:
     ID 1 keyboard - modifiers, reserved, 6 keycodes   -> 9 bytes
     ID 2 mouse    - buttons, X, Y, wheel              -> 5 bytes       */
static const uint8_t hid_report_desc[HID_REPORT_DESC_LEN] = {
  0x05, 0x01,       /* Usage Page (Generic Desktop)          */
  0x09, 0x06,       /* Usage (Keyboard)                      */
  0xA1, 0x01,       /* Collection (Application)              */
  0x85, 0x01,       /*   Report ID (1)                       */
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

  0x05, 0x01,       /* Usage Page (Generic Desktop)          */
  0x09, 0x02,       /* Usage (Mouse)                         */
  0xA1, 0x01,       /* Collection (Application)              */
  0x85, 0x02,       /*   Report ID (2)                       */
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

_Static_assert(sizeof(hid_report_desc) == HID_REPORT_DESC_LEN,
               "HID_REPORT_DESC_LEN does not match hid_report_desc");

static const uint8_t lang_desc[4] = { 4, DESC_STRING, 0x09, 0x04 };  /* en-US */

static const char str_manufacturer[] = "STMicroelectronics";
static const char str_product[]      = "STM32 Bare Metal HID";
static const char str_serial[]       = "BARE00000001";

/* String descriptors are UTF-16LE, built on demand into this buffer. */
static uint8_t str_buf[64];

static uint16_t make_string_desc(const char *s)
{
  uint16_t n = 2U;

  while ((*s != '\0') && (n < (uint16_t)(sizeof(str_buf) - 1U))) {
    str_buf[n++] = (uint8_t)*s++;
    str_buf[n++] = 0x00U;
  }

  str_buf[0] = (uint8_t)n;
  str_buf[1] = DESC_STRING;
  return n;
}

/* --------------------------------------------------------- transfers ----- */

typedef struct {
  uint8_t  bmRequestType;
  uint8_t  bRequest;
  uint16_t wValue;
  uint16_t wIndex;
  uint16_t wLength;
} setup_packet;

enum { CTL_IDLE = 0, CTL_IN_DATA, CTL_STATUS_IN };

static const uint8_t *ctl_ptr;
static uint16_t       ctl_left;
static uint8_t        ctl_zlp;
static uint8_t        ctl_state;

static uint8_t  pending_address;
static uint8_t  have_pending_address;
static uint8_t  configured;
static uint8_t  hid_idle_rate;
static uint8_t  hid_protocol = 1U;

static volatile uint8_t ep1_busy;

static void ctl_send_chunk(void)
{
  uint16_t n = (ctl_left > EP0_SIZE) ? EP0_SIZE : ctl_left;

  pma_write(EP0_TX_OFF, ctl_ptr, n);
  BT_COUNT_TX(0) = n;
  ctl_ptr  += n;
  ctl_left  = (uint16_t)(ctl_left - n);
  ep_set_tx_status(0, EP_TX_VALID);
}

/* IN data stage, truncated to what the host asked for */
static void ctl_send(const uint8_t *data, uint16_t len, uint16_t requested)
{
  if (len > requested) {
    len = requested;
  }

  ctl_ptr   = data;
  ctl_left  = len;
  /* A transfer shorter than requested but an exact multiple of the packet
     size needs a zero length packet to tell the host it is over. */
  ctl_zlp   = ((len < requested) && (len != 0U) && ((len % EP0_SIZE) == 0U)) ? 1U : 0U;
  ctl_state = CTL_IN_DATA;
  ctl_send_chunk();
}

/* zero length IN packet: the status stage of a request with no data */
static void ctl_status(void)
{
  BT_COUNT_TX(0) = 0U;
  ctl_state = CTL_STATUS_IN;
  ep_set_tx_status(0, EP_TX_VALID);
}

static void ctl_stall(void)
{
  ctl_state = CTL_IDLE;
  ep_set_tx_status(0, EP_TX_STALL);
  ep_set_rx_status(0, EP_RX_VALID);
}

static void open_hid_endpoint(void)
{
  BT_ADDR_TX(1)  = EP1_TX_OFF;
  BT_COUNT_TX(1) = 0U;

  EPR(1) = (uint16_t)(EP_TYPE_INTERRUPT | 1U);   /* EA = 1 */
  ep_set_rx_status(1, EP_RX_DIS);
  ep_set_tx_status(1, EP_TX_NAK);
  ep1_busy = 0U;
}

static void handle_get_descriptor(const setup_packet *s)
{
  uint8_t type  = (uint8_t)(s->wValue >> 8);
  uint8_t index = (uint8_t)(s->wValue);

  switch (type) {
  case DESC_DEVICE:
    ctl_send(device_desc, sizeof(device_desc), s->wLength);
    break;

  case DESC_CONFIG:
    ctl_send(config_desc, sizeof(config_desc), s->wLength);
    break;

  case DESC_STRING:
    switch (index) {
    case 0:
      ctl_send(lang_desc, sizeof(lang_desc), s->wLength);
      break;
    case 1:
      ctl_send(str_buf, make_string_desc(str_manufacturer), s->wLength);
      break;
    case 2:
      ctl_send(str_buf, make_string_desc(str_product), s->wLength);
      break;
    case 3:
      ctl_send(str_buf, make_string_desc(str_serial), s->wLength);
      break;
    default:
      ctl_stall();
      break;
    }
    break;

  case DESC_HID:
    /* the 9-byte HID class descriptor sits inside the configuration */
    ctl_send(&config_desc[18], 9U, s->wLength);
    break;

  case DESC_HID_REPORT:
    /* 101 bytes: this is the transfer that needs more than one packet */
    ctl_send(hid_report_desc, sizeof(hid_report_desc), s->wLength);
    break;

  default:
    ctl_stall();
    break;
  }
}

static void handle_standard(const setup_packet *s)
{
  static uint8_t reply[2];

  switch (s->bRequest) {
  case 0x00: /* GET_STATUS */
    reply[0] = 0U;
    reply[1] = 0U;
    ctl_send(reply, 2U, s->wLength);
    break;

  case 0x01: /* CLEAR_FEATURE */
  case 0x03: /* SET_FEATURE */
    ctl_status();
    break;

  case 0x05: /* SET_ADDRESS - applied after the status stage, not now */
    pending_address      = (uint8_t)(s->wValue & 0x7FU);
    have_pending_address = 1U;
    ctl_status();
    break;

  case 0x06: /* GET_DESCRIPTOR */
    handle_get_descriptor(s);
    break;

  case 0x08: /* GET_CONFIGURATION */
    reply[0] = configured;
    ctl_send(reply, 1U, s->wLength);
    break;

  case 0x09: /* SET_CONFIGURATION */
    configured = (uint8_t)(s->wValue & 0xFFU);
    if (configured != 0U) {
      open_hid_endpoint();
    }
    ctl_status();
    break;

  case 0x0A: /* GET_INTERFACE */
    reply[0] = 0U;
    ctl_send(reply, 1U, s->wLength);
    break;

  case 0x0B: /* SET_INTERFACE */
    ctl_status();
    break;

  default:
    ctl_stall();
    break;
  }
}

static void handle_class(const setup_packet *s)
{
  static uint8_t reply[1];

  switch (s->bRequest) {
  case 0x02: /* GET_IDLE */
    reply[0] = hid_idle_rate;
    ctl_send(reply, 1U, s->wLength);
    break;

  case 0x03: /* GET_PROTOCOL */
    reply[0] = hid_protocol;
    ctl_send(reply, 1U, s->wLength);
    break;

  case 0x0A: /* SET_IDLE */
    hid_idle_rate = (uint8_t)(s->wValue >> 8);
    ctl_status();
    break;

  case 0x0B: /* SET_PROTOCOL */
    hid_protocol = (uint8_t)(s->wValue);
    ctl_status();
    break;

  default:
    /* GET_REPORT and SET_REPORT: no output report is declared, so stall.
       Hosts fall back to the interrupt endpoint, which is what we want. */
    ctl_stall();
    break;
  }
}

static void handle_setup(void)
{
  uint8_t raw[8];
  setup_packet s;

  pma_read(EP0_RX_OFF, raw, 8U);

  s.bmRequestType = raw[0];
  s.bRequest      = raw[1];
  s.wValue        = (uint16_t)(raw[2] | ((uint16_t)raw[3] << 8));
  s.wIndex        = (uint16_t)(raw[4] | ((uint16_t)raw[5] << 8));
  s.wLength       = (uint16_t)(raw[6] | ((uint16_t)raw[7] << 8));

  ctl_state = CTL_IDLE;
  ctl_left  = 0U;
  ctl_zlp   = 0U;

  switch (s.bmRequestType & 0x60U) {
  case 0x00U:
    handle_standard(&s);
    break;
  case 0x20U:
    handle_class(&s);
    break;
  default:
    ctl_stall();
    break;
  }
}

static void ep0_in_done(void)
{
  switch (ctl_state) {
  case CTL_IN_DATA:
    if (ctl_left != 0U) {
      ctl_send_chunk();
    } else if (ctl_zlp != 0U) {
      ctl_zlp = 0U;
      BT_COUNT_TX(0) = 0U;
      ep_set_tx_status(0, EP_TX_VALID);
    } else {
      ctl_state = CTL_IDLE;
      ep_set_rx_status(0, EP_RX_VALID);
    }
    break;

  case CTL_STATUS_IN:
    /* The address only takes effect once the host has seen the status ZLP */
    if (have_pending_address != 0U) {
      USB->DADDR = (uint16_t)(USB_DADDR_EF | pending_address);
      have_pending_address = 0U;
    }
    ctl_state = CTL_IDLE;
    ep_set_rx_status(0, EP_RX_VALID);
    break;

  default:
    ep_set_rx_status(0, EP_RX_VALID);
    break;
  }
}

/* ------------------------------------------------------------- driver ---- */

static void usb_reset(void)
{
  configured = 0U;
  ctl_state  = CTL_IDLE;
  ep1_busy   = 0U;
  have_pending_address = 0U;

  USB->BTABLE = 0U;

  BT_ADDR_TX(0)  = EP0_TX_OFF;
  BT_COUNT_TX(0) = 0U;
  BT_ADDR_RX(0)  = EP0_RX_OFF;
  BT_COUNT_RX(0) = rx_blocks(EP0_SIZE);

  EPR(0) = (uint16_t)(EP_TYPE_CONTROL | 0U);    /* EA = 0 */
  ep_set_rx_status(0, EP_RX_VALID);
  ep_set_tx_status(0, EP_TX_NAK);

  /* Answer on address 0 until SET_ADDRESS arrives */
  USB->DADDR = USB_DADDR_EF;
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
  uint16_t istr = USB->ISTR;

  if ((istr & USB_ISTR_RESET) != 0U) {
    USB->ISTR = (uint16_t)~USB_ISTR_RESET;
    usb_reset();
    return;
  }

  if ((istr & USB_ISTR_SUSP) != 0U) {
    USB->ISTR = (uint16_t)~USB_ISTR_SUSP;
    USB->CNTR |= USB_CNTR_FSUSP;
  }

  if ((istr & USB_ISTR_WKUP) != 0U) {
    USB->ISTR  = (uint16_t)~USB_ISTR_WKUP;
    USB->CNTR &= (uint16_t)~USB_CNTR_FSUSP;
  }

  /* Drain every completed transfer: ISTR latches one per endpoint event */
  while (((istr = USB->ISTR) & USB_ISTR_CTR) != 0U) {
    uint8_t ep = (uint8_t)(istr & USB_ISTR_EP_ID);

    if (ep == 0U) {
      if ((istr & USB_ISTR_DIR) == 0U) {
        /* IN: a packet we queued has been read by the host */
        ep_clear_ctr_tx(0);
        ep0_in_done();
      } else {
        uint16_t epr = EPR(0);

        if ((epr & USB_EP0R_SETUP) != 0U) {
          ep_clear_ctr_rx(0);
          handle_setup();
        } else {
          /* OUT: the status stage of an IN transfer, nothing to read */
          ep_clear_ctr_rx(0);
          ep_set_rx_status(0, EP_RX_VALID);
        }
      }
    } else {
      ep_clear_ctr_tx(ep);
      if (ep == 1U) {
        ep1_busy = 0U;
      }
    }
  }
}

void USBWakeUp_IRQHandler(void)
{
  EXTI->PR = EXTI_PR_PR18;
}

void usb_init(void)
{
  RCC->APB1ENR |= RCC_APB1ENR_USBEN;

  /* Leave power down, hold the peripheral in reset for the startup time */
  USB->CNTR = USB_CNTR_FRES;
  for (volatile uint32_t i = 0U; i < 1000U; i++) {
  }

  USB->CNTR  = 0U;                 /* release reset */
  USB->ISTR  = 0U;                 /* clear every pending event */
  USB->BTABLE = 0U;

  USB->CNTR = USB_CNTR_RESETM | USB_CNTR_CTRM | USB_CNTR_SUSPM | USB_CNTR_WKUPM;

  NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 0U);
  NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
}

int usb_configured(void)
{
  return (configured != 0U) ? 1 : 0;
}

int usb_hid_ready(void)
{
  return ((configured != 0U) && (ep1_busy == 0U)) ? 1 : 0;
}

int usb_hid_send(const uint8_t *report, uint16_t len)
{
  if ((configured == 0U) || (ep1_busy != 0U) || (len > USB_HID_EP_SIZE)) {
    return -1;
  }

  ep1_busy = 1U;
  pma_write(EP1_TX_OFF, report, len);
  BT_COUNT_TX(1) = len;
  ep_set_tx_status(1, EP_TX_VALID);

  return 0;
}
