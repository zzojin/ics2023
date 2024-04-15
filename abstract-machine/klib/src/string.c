#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

size_t strlen(const char *s) {
  const char *c = s;
  size_t len = 0;
  while (*c != '\0') {
    len++;
    c++;
  }
  return len;
}

char *strcpy(char *dst, const char *src) {
  char *res = dst;
  for (const char *c = src; *c != '\0'; c++) {
      *(dst++) = *c;
  }
  *dst = '\0';
  return res;
}

char *strncpy(char *dst, const char *src, size_t n) {
  char *res = dst;
  size_t k = 0;
  for (const char *c = src; *c != '\0' && k < n; c++) {
      *(dst++) = *c;
      k++;
  }
  if (k < n)
    *dst = '\0';
  return res;
}

char *strcat(char *dst, const char *src) {
  char *c = dst;
  while (*c != '\0') {
      c++;
  }
  for (; *src != '\0'; src++) {
      *c = *src;
      c++;
  }
  *c = '\0';
  return dst;
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 != '\0' && *s2 != '\0') {
      if (*s1 > *s2)
          return 1;
      else if (*s1 < *s2)
          return -1;
      else {
          s1++;
          s2++;
      }
  }
  if (*s1 == '\0' && *s2 == '\0')
      return 0;
  else if (*s1 == '\0')
      return -1;
  else 
      return 1;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  while (*s1 != '\0' && *s2 != '\0') {
      if (*s1 > *s2)
          return 1;
      else if (*s1 < *s2)
          return -1;
      else {
          s1++;
          s2++;
      }
  }
  if (*s1 == *s2)
      return 0;
  else if (*s1 < *s2)
      return -1;
  else 
      return 1;
}

void *memset(void *s, int c, size_t n) {
  char ch = (char) c;
  char *dst = (char *)s;
  for (size_t i = 0; i < n; i++) {
      *(dst + i) = ch;
  }
  return s;
}

void *memmove(void *dst, const void *src, size_t n) {
    char *d = (char *)dst;
    const char *s = (const char *)src;
    if (d <= s || d >= s + n) {
      while (n--) {
          *d++ = *s++;
      }
    }
    else {
      d += n - 1;
      s += n - 1;
      while (n--) {
          *d-- = *s--;
      }
    }
    return dst;
}

void *memcpy(void *out, const void *in, size_t n) {
  char *o = (char *)out;
  const char *i = (const char *)in;
  while (n--) {
      *o++ = *i++;
  }
  return out;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    char *p1 = (char *)s1;
    const char *p2 = (const char *)s2;
    while (n--) {
        if (*p1 != *p2) {
            return (*p1 > *p2) ? 1 : -1;
        }
        p1++;
        p2++;
    }
    return 0;
}

#endif
