#include <am.h>
#include <nemu.h>
#include "amdev.h"

#define KEYDOWN_MASK 0x8000

void __am_input_keybrd(AM_INPUT_KEYBRD_T *kbd) {
  uint32_t keycode = inl(KBD_ADDR);
  kbd->keycode = keycode & ~KEYDOWN_MASK;
  kbd->keydown = keycode & KEYDOWN_MASK ? true: false;
}
