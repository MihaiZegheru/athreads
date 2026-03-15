#ifndef DEBUG_H__
#define DEBUG_H__

#include "usart.h"

#ifdef INFO
#define LOG_INFO(fmt, ...) do { \
    usart_printf("[INFO] " fmt "\r\n", ##__VA_ARGS__); \
} while(0)

#define LOG_ERROR(fmt, ...) do { \
    usart_printf("[ERROR] " fmt "\r\n", ##__VA_ARGS__); \
} while(0)
#else
#define LOG_INFO(fmt, ...) do { } while(0)
#define LOG_ERROR(fmt, ...) do { } while(0)
#endif

#ifdef DEBUG
#define LOG_DEBUG(fmt, ...) do { \
    usart_printf("[DEBUG] " fmt "\r\n", ##__VA_ARGS__); \
} while(0)
#else
#define LOG_DEBUG(fmt, ...) do { } while(0)
#endif

#endif // DEBUG_H__
