#include "hid.h"
#include "usb.h"
#include "board.h"

/* Waits for EP1 IN to drain so back to back reports are not dropped. */
static int wait_ready(uint32_t timeout_ms)
{
  uint32_t start = millis();

  while (usb_hid_ready() == 0) {
    if ((millis() - start) > timeout_ms) {
      return -1;
    }
  }
  return 0;
}

int hid_mouse_send(int8_t x, int8_t y, int8_t wheel, uint8_t buttons)
{
  uint8_t report[USB_HID_MOUSE_REPORT_LEN];

  report[0] = HID_REPORT_ID_MOUSE;
  report[1] = buttons;
  report[2] = (uint8_t)x;
  report[3] = (uint8_t)y;
  report[4] = (uint8_t)wheel;

  return usb_hid_send(report, sizeof(report));
}

int hid_mouse_click(uint8_t buttons)
{
  if (wait_ready(50U) != 0) {
    return -1;
  }
  (void)hid_mouse_send(0, 0, 0, buttons);

  delay_ms(2U * USB_HID_EP_INTERVAL_MS);

  if (wait_ready(50U) != 0) {
    return -1;
  }
  return hid_mouse_send(0, 0, 0, 0U);
}

int hid_keyboard_send(uint8_t modifiers, const uint8_t keys[6])
{
  uint8_t report[USB_HID_KEYBOARD_REPORT_LEN];
  uint8_t i;

  report[0] = HID_REPORT_ID_KEYBOARD;
  report[1] = modifiers;
  report[2] = 0U;                       /* reserved */

  for (i = 0U; i < 6U; i++) {
    report[3U + i] = (keys != 0) ? keys[i] : 0U;
  }

  return usb_hid_send(report, sizeof(report));
}

int hid_keyboard_release(void)
{
  return hid_keyboard_send(0U, 0);
}

int hid_keyboard_tap(uint8_t modifiers, uint8_t key)
{
  uint8_t keys[6] = { 0U, 0U, 0U, 0U, 0U, 0U };

  keys[0] = key;

  if (wait_ready(50U) != 0) {
    return -1;
  }
  (void)hid_keyboard_send(modifiers, keys);

  delay_ms(2U * USB_HID_EP_INTERVAL_MS);

  if (wait_ready(50U) != 0) {
    return -1;
  }
  return hid_keyboard_release();
}

/* Maps 7-bit ASCII to a US layout usage ID, 0 when there is none. */
static uint8_t ascii_to_key(char c, uint8_t *modifiers)
{
  *modifiers = 0U;

  if ((c >= 'a') && (c <= 'z')) {
    return (uint8_t)(HID_KEY_A + (c - 'a'));
  }

  if ((c >= 'A') && (c <= 'Z')) {
    *modifiers = HID_MOD_LSHIFT;
    return (uint8_t)(HID_KEY_A + (c - 'A'));
  }

  if ((c >= '1') && (c <= '9')) {
    return (uint8_t)(HID_KEY_1 + (c - '1'));
  }

  switch (c) {
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

  /* Characters that need shift on a US layout */
  *modifiers = HID_MOD_LSHIFT;

  switch (c) {
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

void hid_keyboard_type(const char *str)
{
  uint8_t prev_key = HID_KEY_NONE;

  if (str == 0) {
    return;
  }

  while (*str != '\0') {
    uint8_t modifiers;
    uint8_t key = ascii_to_key(*str, &modifiers);

    if (key != HID_KEY_NONE) {
      /* The same key twice in a row needs a release in between, or the host
         sees one long press instead of two taps. */
      if ((key == prev_key) && (wait_ready(50U) == 0)) {
        (void)hid_keyboard_release();
        delay_ms(USB_HID_EP_INTERVAL_MS);
      }

      (void)hid_keyboard_tap(modifiers, key);
      prev_key = key;
    }

    str++;
  }

  if (wait_ready(50U) == 0) {
    (void)hid_keyboard_release();
  }
}
