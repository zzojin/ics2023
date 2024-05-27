#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

static int evtdev = -1;
static int fbdev = -1;
static int screen_w = 0, screen_h = 0;
static int canvas_x = 0, canvas_y = 0;

uint32_t NDL_GetTicks() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// 读出一条事件信息, 将其写入`buf`中, 最长写入`len`字节
// 若读出了有效的事件, 函数返回1, 否则返回0
int NDL_PollEvent(char *buf, int len) {
    if (read(evtdev, buf, len) > 0) {
        return 1;
    }
    else {
        return 0;
    }
}

void NDL_OpenCanvas(int *w, int *h) {
    int fd = open("/proc/dispinfo", O_RDONLY);
    struct {int w, h;} cfg;
    read(fd, &cfg, sizeof(cfg));
    close(fd);
    screen_w = cfg.w;
    screen_h = cfg.h;
    if (*w == 0 && *h == 0 || *w > screen_w || *h > screen_h) {
        *w = screen_w;
        *h = screen_h;
    }
    // 画布左上角点位于屏幕居中位置
    canvas_x = (screen_w - *w) / 2;
    canvas_y = (screen_h - *h) / 2;
    printf("[NDL_OpenCanvas]: screen width=%d, screen height=%d, canvas w=%d, canvas h=%d\n", screen_w, screen_h, *w, *h);

    if (getenv("NWM_APP")) {
        int fbctl = 4;
        fbdev = 5;
        screen_w = *w; screen_h = *h;
        char buf[64];
        int len = sprintf(buf, "%d %d", screen_w, screen_h);
        // let NWM resize the window and create the frame buffer
        write(fbctl, buf, len);
        while (1) {
          // 3 = evtdev
          int nread = read(3, buf, sizeof(buf) - 1);
          if (nread <= 0) continue;
          buf[nread] = '\0';
          if (strcmp(buf, "mmap ok") == 0) break;
        }
        close(fbctl);
    }
}

void NDL_DrawRect(uint32_t *pixels, int x, int y, int w, int h) {
    int graphics = open("/dev/fb", O_RDWR);
    uint32_t *line = pixels;
    for (int i = 0; i < h; ++i){
        // 在画布内的第 y 行开始写入，循环到第 i 次时，对于屏幕而言，当前写入的位置就是 canvas_y + y + i 行
        lseek(graphics, ((canvas_y + y + i) * screen_w + (canvas_x + x)) * sizeof(uint32_t), SEEK_SET);
        ssize_t s = write(graphics, line, w * sizeof(uint32_t));
        line += w;          // 完成了一行的写入，更新下一次的写入点
    }
}

void NDL_OpenAudio(int freq, int channels, int samples) {
}

void NDL_CloseAudio() {
}

int NDL_PlayAudio(void *buf, int len) {
  return 0;
}

int NDL_QueryAudio() {
  return 0;
}

int NDL_Init(uint32_t flags) {
  if (getenv("NWM_APP")) {
    evtdev = 3;
  }
  //init_ticks = NDL_GetTicks_internal();
  evtdev = open("/dev/events", O_RDONLY);
  assert(evtdev);
  fbdev = open("/dev/fb", O_WRONLY);
  assert(fbdev);
  return 0;
}

void NDL_Quit() {
}
