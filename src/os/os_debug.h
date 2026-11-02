/*!
    \file    src/os/os_debug.h
    \brief   AeroDENS OS-Debug Mode (RT-Thread Multi-Thread + GT911 Touch Canvas Demo)
             Only launched when User Key (PF8) is held during boot before OS runs.
*/

#ifndef AERODENS_OS_DEBUG_H
#define AERODENS_OS_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

void os_debug_start(void);

#ifdef __cplusplus
}
#endif

#endif /* AERODENS_OS_DEBUG_H */

