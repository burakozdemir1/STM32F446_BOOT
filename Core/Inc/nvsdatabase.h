/*
 * nvsdatabase.h
 *
 *  Created on: Aug 16, 2025
 *      Author: Burak Ozdemir
 */

#ifndef INC_NVSDATABASE_H_
#define INC_NVSDATABASE_H_

#include "main.h"
#include "stm32f4xx_hal.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define NS_DB_SSID_MAX   (32U)
#define NS_DB_PASS_MAX   (64U)
#define FLASH_PAGE_SIZE          (2048u)
/* Son iki sayfayı kullan: Page 62 ve Page 63 */
#define NS_DB_FLASH_SSID_ADDR    ((uint32_t)0x0801F000)  /* Page 62 başlangıcı */
#define NS_DB_FLASH_PASS_ADDR    ((uint32_t)0x0801F800)  /* Page 63 başlangıcı (AYRI SAYFA) */


typedef enum {

	NS_DB_STATUS_WRITE_OK,
	NS_DB_STATUS_WRITE_FAIL,
	NS_DB_STATUS_READ_OK,
	NS_DB_STATUS_READ_FAIL,
	NS_DB_STATUS_SUCCESS,
	NS_DB_STATUS_FAIL,
	NS_DB_STATUS_TIMEOUT

} NVS_DB_STATUS;


typedef struct
{
    volatile bool databaseApModeGetJSON;  /* AP tarafı JSON geldiğinde set eder */
    volatile bool credentialsReady;       /* Parse başarılı olduysa true */

    char ssid[NS_DB_SSID_MAX + 1];
    char pass[NS_DB_PASS_MAX + 1];

} NVS_DB_HANDLE;


NVS_DB_STATUS nvsdatabase_writeData(void);
NVS_DB_STATUS nvsdatabase_readData(void);
NVS_DB_STATUS nvsdatabase_validate(void);
NVS_DB_STATUS nvsdatabase_flashReadCharData(uint32_t address, char *buffer, uint32_t maxLength);
extern NVS_DB_HANDLE databaseHandle;




#endif /* INC_NVSDATABASE_H_ */
