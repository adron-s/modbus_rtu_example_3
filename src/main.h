#ifndef __MAIN_H
#define __MAIN_H
#include "stm32f0xx_hal.h"

void set_led_pin_state(int);
int get_led_pin_state(void);

/* Debug control. */
#define DEBUG_PRINTF 1
#define DEBUG_PRINTE 1
#define DEBUG_SERIAL_DATA_FLOWS 1

/* Prints debug messages if debugging is enabled. */
#if (defined DEBUG_PRINTF) && (DEBUG_PRINTF == 1)
#define printd(format, args...)                                                \
	printf("%s(%d):: " format, __func__, __LINE__, ##args)
#else
#define printd(...)
#endif
/* Prints error messages if error debugging is enabled. */
#if (defined DEBUG_PRINTE) && (DEBUG_PRINTE == 1)
#define printe(format, args...)                                                \
	printf("!!! ERROR: %s(%d):: " format " !!!\n", __func__, __LINE__, ##args)
#else
#define printe(...)
#endif

/* Prints (byte by byte) the contents of the passed buffer of the given size. */
#define data_print(buf, buf_size)                                              \
	{                                                                      \
		printf("%s:: data:", __func__);                                \
		for (int i = 0; i < buf_size; i++) {                           \
			printf(" %02X", buf[i]);                               \
		}                                                              \
		printf("\n");                                                  \
	}

/*
	Performs a two-byte swap if the current system is little-endian.
	This is necessary to convert Modbus 16 bit register values ​​(which are
	always in big-endian form) to the current system's byte order.
*/
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define SWAP16_IF_LE(x) __REV16(x)
#else
#define SWAP16_IF_LE(x) (x)
#endif

#endif /* __MAIN_H */
