#include "riscv/riscv.h"
#include <am.h>
#include <nemu.h>
#include <stdint.h>

#define SYNC_ADDR (VGACTL_ADDR + 4)

void __am_gpu_init() {
   uint32_t screen_wh = inl(VGACTL_ADDR);
   uint32_t h = screen_wh & 0xffff;
   uint32_t w = screen_wh >> 16;
   int i;
   uint32_t *fb = (uint32_t *)(uintptr_t)FB_ADDR;
   for (i = 0; i < w * h; i ++) fb[i] = i;
   outl(SYNC_ADDR, 1);
}

void __am_gpu_config(AM_GPU_CONFIG_T *cfg) {
  *cfg = (AM_GPU_CONFIG_T) {
    .present = true, .has_accel = false,
    .width = 0, .height = 0,
    .vmemsz = 0
  };
  uint32_t w_h = inl(VGACTL_ADDR);
  cfg->width = (int)(w_h >> 16);
  cfg->height = (int) (w_h & 0xffff);
  cfg->vmemsz = cfg->height * cfg->width * sizeof(uint32_t);
}

// 客户程序构造 AM_GPU_FBDRAW_T 结构体，调用 __am_gpu_fbdraw 完成对显存的修改.进而 NEMU 每次 CPU 执行完之后调用 vga_update_screen，完成硬件显示
void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *ctl) {
  /*int x = ctl->x, y = ctl->y, w = ctl->w, h = ctl->h;
   *if (!ctl->sync && (w == 0 || h == 0)) return;
   *uint32_t *pixels = ctl->pixels;
   *uint32_t *fb = (uint32_t *)(uintptr_t)FB_ADDR;        // FB_ADDR 的原始数值是定义在 nemu.h 的一个宏，并不是一个地址，所以先转成地址，然后再赋予地址类型
   *uint32_t screen_w = inl(VGACTL_ADDR) >> 16;
   *for (int i = y; i < y+h; i++) {
   *  for (int j = x; j < x+w; j++) {
   *    fb[screen_w*i+j] = pixels[w*(i-y)+(j-x)];
   *  }
   *}*/
  if (ctl->sync) {
    outl(SYNC_ADDR, 1);
  }
}

void __am_gpu_status(AM_GPU_STATUS_T *status) {
  status->ready = true;
}
