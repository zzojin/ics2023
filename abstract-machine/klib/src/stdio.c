#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

static char HEX_CHARACTERS[] = "0123456789abcdef";

static int int2str(char *buffer, int num) {
    int neg = 0;
    if (num == 0) {
        buffer[0] = '0';
        return 1;
    } else if (num < 0)
        neg = 1;
    int i = 0;
    // 当 num 是负数时，直接给 num 取相反数可能会导致溢出，例如[-256,255] 中的 -256.
    // 因此，求模之后根据余数是否为负数，调整符号
    int remainder;
    for (; num; num /= 10) {
        remainder = num % 10;
        if (neg)
            remainder = -remainder;
        buffer[i++] = HEX_CHARACTERS[remainder % 10];
    }
    if (neg) {
        buffer[i++] = '-';
    }
    return i;
}

static int pointer2hex(char *buffer, size_t p) {
    int remainder;
    int i = 0;
    for (; p; p /= 16) {
        remainder = p % 16;
        buffer[i++] = HEX_CHARACTERS[remainder % 16];
    }
    // 补 0
    for (int k = i; k <= sizeof(size_t) * 2; k++) {
        buffer[i++] = '0';
    }
    buffer[i++] = 'x';
    buffer[i++] = '0';

    return i;
}

static int uint2hex(char *buffer, unsigned int u) {
    int remainder;
    int i = 0;
    do {
        remainder = u % 16;
        buffer[i++] = HEX_CHARACTERS[remainder % 16];
    } while (u /= 16);
/*    buffer[i++] = 'x';*/
    /*buffer[i++] = '0';*/
    return i;
}


int printf(const char *fmt, ...) {
  panic("Not implemented");
}

int vsprintf(char *out, const char *fmt, va_list ap) {
    return vsnprintf(out, -1, fmt, ap);
}

int sprintf(char *out, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    return vsnprintf(out, -1, fmt, ap);
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    return vsnprintf(out, n, fmt, ap);
}

#define append(x) do {out[j++]=x; } while(0)
int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
    int state = 0;
    int j = 0, num;
    char c, ch;
    char *s;
    size_t p;
    char buffer[50];
    unsigned int length;
    unsigned int u;
    while (c = *fmt++, c && n > 0 && j < n - 1) {
        switch (state) {
            case 0:
                if (c == '%')
                    state = 1;
                else
                    append(c);
                break;
            case 1:
                switch (c) {
                    case '%':
                        append(c);
                        break;
                    case 'd':
                        num = va_arg(ap, int);
                        length = int2str(buffer, num);
                        for (int i = length - 1; i >= 0; i--) {
                            append(buffer[i]);
                        }
                        break;
                    case 'c':
                        // char type promote to int
                        ch = va_arg(ap, int);
                        append(ch);
                        break;
                    case 's':
                        s = va_arg(ap, char*);
                        for (int i = 0; s[i] != '\0'; i++) {
                            append(s[i]);
                        }
                        break;
                    case 'p':
                        p = va_arg(ap, size_t);
                        length = pointer2hex(buffer, p); 
                        for (int i = length - 1; i >= 0; i--) {
                            append(buffer[i]);
                        }
                        break;
                    case 'x':
                        u = va_arg(ap, unsigned int);
                        length = uint2hex(buffer, u); 
                        for (int i = length - 1; i >= 0; i--) {
                            append(buffer[i]);
                        }
                        break;
                    default:
                        ;
                        //Log("Type %%%c doesn't be supported ", c);
                }
                state = 0;
                break;
        }
    }
    va_end(ap);
    out[j] = '\0';
    return j;
}
#undef append
#endif
