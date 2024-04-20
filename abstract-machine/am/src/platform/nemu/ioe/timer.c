#include "riscv/riscv.h"
#include <am.h>
#include <nemu.h>
#include <stdint.h>

void __am_timer_init() {
}

void __am_timer_uptime(AM_TIMER_UPTIME_T *uptime) {
    // 必须先读高位再读低位，因为读取时间的过程需要两次访存，所以自然状况下有两次 gettime。NEMU 经过特殊设计，只有在 offset = 4，即访问高位的时候才触发 gettime
    // 因此必须先访问高位 high，更新最新的时间，然后第二次访问 low 就不会更新时间了，直接从上次记录的时间数据里取出低位。
    uint32_t high = inl(RTC_ADDR + 4);
    uint32_t low = inl(RTC_ADDR);
    uptime->us = (uint64_t)high << 32 | (uint64_t)low;
}

void __am_timer_rtc(AM_TIMER_RTC_T *rtc) {
  rtc->second = 0;
  rtc->minute = 0;
  rtc->hour   = 0;
  rtc->day    = 0;
  rtc->month  = 0;
  rtc->year   = 1900;
}
