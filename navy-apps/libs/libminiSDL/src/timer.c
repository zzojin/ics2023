#include <NDL.h>
#include <sdl-timer.h>
#include <stdio.h>

#define MAX_DAYS (49 * 24 * 3600 * 1000u)

SDL_TimerID SDL_AddTimer(uint32_t interval, SDL_NewTimerCallback callback, void *param) {
  return NULL;
}

int SDL_RemoveTimer(SDL_TimerID id) {
  return 1;
}

uint32_t SDL_GetTicks() {
    uint32_t ret = NDL_GetTicks();
    if (ret > MAX_DAYS)
        return -1;
    return ret;
}

void SDL_Delay(uint32_t ms) {
}
