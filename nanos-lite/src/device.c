#include "am.h"
#include "amdev.h"
#include "fs.h"
#include "klib-macros.h"
#include <common.h>
#include <stdio.h>

#if defined(MULTIPROGRAM) && !defined(TIME_SHARING)
# define MULTIPROGRAM_YIELD() yield()
#else
# define MULTIPROGRAM_YIELD()
#endif

#define NAME(key) \
  [AM_KEY_##key] = #key,

static const char *keyname[256] __attribute__((used)) = {
  [AM_KEY_NONE] = "NONE",
  AM_KEYS(NAME)
};

size_t serial_write(const void *buf, size_t offset, size_t len) {
    char *str = (char *)buf;
    for (int i = 0; i < len; i++){
        putch(str[i]);
    }
    return len;
}

size_t events_read(void *buf, size_t offset, size_t len) {
    AM_INPUT_KEYBRD_T ev = io_read(AM_INPUT_KEYBRD);
    if (ev.keycode == AM_KEY_NONE) {
        ((char *)buf)[0] = '\0';
        return 0;
    } 

    int ret = snprintf(buf, len, "%s %s\n", ev.keydown ? "kd" : "ku", keyname[ev.keycode]);
    return ret;
}

size_t dispinfo_read(void *buf, size_t offset, size_t len) {
    AM_GPU_CONFIG_T gpu_cfg = io_read(AM_GPU_CONFIG);
    int *b = (int *)buf;
    *b++ = gpu_cfg.width;
    *b++ = gpu_cfg.height;
    return (char *)b - (char *)buf;
}

size_t fb_write(const void *buf, size_t offset, size_t len) {
    AM_GPU_CONFIG_T cfg = io_read(AM_GPU_CONFIG);
    int x = (offset / 4) % cfg.width;               // 行内偏移
    int y = (offset / 4) / cfg.width;               // 行号
    
    // 超出屏幕范围
    if (len + offset > cfg.width * cfg.height * 4) 
        len = cfg.width*cfg.height*4 - offset;

    // amdev.h AM_DEVREG(11, GPU_FBDRAW,   WR, int x, y; void *pixels; int w, h; bool sync);
    // 写入一行
    io_write(AM_GPU_FBDRAW, x, y, (uint32_t*)buf, len / 4, 1, true);
    return len;
}

void init_device() {
  Log("Initializing devices...");
  ioe_init();
}
