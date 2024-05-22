#include <stdio.h>
#include <NDL.h>

int main() {
  NDL_Init(0);
  while (1) {
    char buf[64];
    int ret = NDL_PollEvent(buf, sizeof(buf));
    if (ret) {
      printf("receive event: %s\n", buf);
    }
  }
  return 0;
}
