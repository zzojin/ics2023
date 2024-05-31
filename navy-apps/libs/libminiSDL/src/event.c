#include <NDL.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>

#define keyname(k) #k,

static const char *keyname[] = {
  "NONE",
  _KEYS(keyname)
};

static char evbuf[64];
static uint8_t key_state[sizeof(keyname) / sizeof(keyname[0])] = {0};

int SDL_PushEvent(SDL_Event *ev) {
  return 0;
}

int SDL_PollEvent(SDL_Event *event) {
  int keynum = sizeof(keyname)/sizeof(char*);
  
    if (NDL_PollEvent(evbuf, sizeof(evbuf))) {
      if (evbuf[1] == 'd') event->key.type = SDL_KEYDOWN;
      else event->key.type = SDL_KEYUP;
      while (keynum--) {
        if (strcmp(keyname[keynum], evbuf+3) == 0) {
          if (event->key.type == SDL_KEYDOWN) {
            key_state[keynum] = 1;
          } else {
            key_state[keynum] = 0;
          }
          event->key.keysym.sym = keynum;
        }
      }
      return 1; 
    } 
  return 0;
}

int SDL_WaitEvent(SDL_Event *event) {
  int keynum = sizeof(keyname)/sizeof(char*);
  while (1) {
    if (NDL_PollEvent(evbuf, sizeof(evbuf))) {
      if (evbuf[1] == 'd') event->key.type = SDL_KEYDOWN;
      else event->key.type = SDL_KEYUP;
      while (keynum--) {
        if (strcmp(keyname[keynum], evbuf+3) == 0) {
          if (event->key.type == SDL_KEYDOWN) {
            key_state[keynum] = 1;
          } else {
            key_state[keynum] = 0;
          }
          event->key.keysym.sym = keynum;
        }
      }
      break;
    } 
  }
  return 1;
}


int SDL_PeepEvents(SDL_Event *ev, int numevents, int action, uint32_t mask) {
  return 0;
}

uint8_t* SDL_GetKeyState(int *numkeys) {
    SDL_Event ev;

    if (numkeys)
        *numkeys = sizeof(key_state) / sizeof(key_state[0]);
    //SDL_PumpEvents();
    return key_state;
}
