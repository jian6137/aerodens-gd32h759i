/*
 * RT-Thread Kernel Services & Console Formatting (src/os/rtthread/kservice.c)
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtthread.h>
#include <rthw.h>
#include <stdio.h>
#include <string.h>

void *rt_memset(void *src, int c, rt_ubase_t n)
{
    return memset(src, c, n);
}

void *rt_memcpy(void *dest, const void *src, rt_ubase_t n)
{
    return memcpy(dest, src, n);
}

rt_int32_t rt_strncmp(const char *cs, const char *ct, rt_ubase_t count)
{
    return strncmp(cs, ct, count);
}

rt_int32_t rt_strcmp(const char *cs, const char *ct)
{
    return strcmp(cs, ct);
}

rt_size_t rt_strlen(const char *src)
{
    return strlen(src);
}

char *rt_strncpy(char *dst, const char *src, rt_ubase_t n)
{
    if (n != 0) {
        char *d = dst;
        const char *s = src;
        do {
            if ((*d++ = *s++) == 0) {
                while (--n != 0) {
                    *d++ = 0;
                }
                break;
            }
        } while (--n != 0);
    }
    return dst;
}

int __rt_ffs(int value)
{
    if (value == 0) {
        return 0;
    }
    return __builtin_ffs(value);
}

rt_int32_t rt_vsnprintf(char *buf, rt_size_t size, const char *fmt, va_list args)
{
    return vsnprintf(buf, size, fmt, args);
}

rt_int32_t rt_snprintf(char *buf, rt_size_t size, const char *fmt, ...)
{
    rt_int32_t n;
    va_list args;

    va_start(args, fmt);
    n = rt_vsnprintf(buf, size, fmt, args);
    va_end(args);

    return n;
}

void rt_kprintf(const char *fmt, ...)
{
    va_list args;
    rt_size_t length;
    static char rt_log_buf[RT_CONSOLEBUF_SIZE];

    va_start(args, fmt);
    length = rt_vsnprintf(rt_log_buf, sizeof(rt_log_buf) - 1, fmt, args);
    if (length > RT_CONSOLEBUF_SIZE - 1) {
        length = RT_CONSOLEBUF_SIZE - 1;
    }
    rt_log_buf[length] = '\0';
    rt_hw_console_output(rt_log_buf);
    va_end(args);
}

void rt_show_version(void)
{
    rt_kprintf("\n \\ | /\n");
    rt_kprintf("- RT -     Thread Operating System (AeroDENS OS @ 0x08010000)\n");
    rt_kprintf(" / | \\     %d.%d.%d build %s\n",
               (int)RT_VERSION, (int)RT_SUBVERSION, (int)RT_REVISION, __DATE__);
    rt_kprintf(" 2006 - 2026 Copyright by rt-thread team\n");
}

