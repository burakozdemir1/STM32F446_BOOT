/*
 * nvice_logger.c
 *
 *  Created on: Jun 7, 2025
 *      Author: Burak Ozdemir
 */

#include "nvslogger.h"


#include <string.h>
#include <stdarg.h>
#include <stdio.h>




void logInfo(const char *format, ...)
{
	static  char buf[412];

    va_list ap;
    va_start(ap, format);
    int n = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);

    if (n < 0) {
        return;
    }

    size_t len = (n >= (int)sizeof(buf)) ? (sizeof(buf) - 1) : (size_t)n;
    size_t sent = 0;
    while (sent < len) {
        size_t chunk = len - sent;
        if (chunk > 64) chunk = 64;

        HAL_StatusTypeDef st = HAL_UART_Transmit(&huart2,
                                                 (uint8_t *)&buf[sent],
                                                 (uint16_t)chunk,
                                                 200);
        if (st != HAL_OK)
            break;
        sent += chunk;
    }
}
