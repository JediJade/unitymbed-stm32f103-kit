#ifndef __USBD_CONF_H
#define __USBD_CONF_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

#define USBD_MAX_NUM_INTERFACES               1
#define USBD_MAX_NUM_CONFIGURATION            1
#define USBD_MAX_STR_DESC_SIZ                 512
#define USBD_DEBUG_LEVEL                      0
#define USBD_LPM_ENABLED                      0
#define USBD_SELF_POWERED                     1

/* Declare custom memory functions from main.c */
void *memset(void *s, int c, uint32_t n);
void *memcpy(void *dest, const void *src, uint32_t n);

/* Static allocation for CDC */
void *USBD_static_malloc(uint32_t size);
void USBD_static_free(void *p);

#define USBD_malloc               USBD_static_malloc
#define USBD_free                 USBD_static_free
#define USBD_memset               memset
#define USBD_memcpy               memcpy
#define USBD_Delay                HAL_Delay

#endif /* __USBD_CONF_H */