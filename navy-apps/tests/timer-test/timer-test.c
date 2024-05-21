#include <stdint.h>
#include <sys/time.h>
#include <stdio.h>
#include <NDL.h>

/*int main() {
 *    struct timezone tz;
 *    struct timeval tv, s_tv;
 *    gettimeofday(&s_tv, &tz);           // 我记得有一个头文件将 _gettimeofday 等一系列 navy-apps 里的系统调用相关调用函数，定别名为 gettimeofday. 就是去掉了前面的下划线。就是为了 navy-apps 的客户程序，能够使用正常的标准库函数名称。
 *    size_t times = 0;
 *    uint64_t prev_us = s_tv.tv_sec * 1000ul * 1000ul + s_tv.tv_usec;
 *    while (gettimeofday(&tv, &tz) == 0) {
 *        if ((tv.tv_usec + 1000000ul * tv.tv_sec - prev_us) > 500000ul) {
 *            printf("%lu * 0.5s\n", times++);
 *            prev_us += 500000ul;
 *        }
 *        
 *    }
 *    return 0;
 *}*/

int main() {
    NDL_Init(0);
    struct timeval tv, s_tv;
    uint32_t t = NDL_GetTicks(&s_tv, NULL);           // 我记得有一个头文件将 _gettimeofday 等一系列 navy-apps 里的系统调用相关调用函数，定别名为 gettimeofday. 就是去掉了前面的下划线。就是为了 navy-apps 的客户程序，能够使用正常的标准库函数名称。
    size_t times = 0;
    uint32_t interval = 500;
    while (1) {
        uint32_t new_time = NDL_GetTicks(&s_tv, NULL);
        if (new_time - t > interval) {
            printf("%lu * 0.5s\n", times++);
            t += 500;
        }
    }
    return 0;
}
