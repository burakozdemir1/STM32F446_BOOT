/*
 * nvsesp8266.h
 *
 *  Created on: Nov 22, 2025
 *      Author: Burak Ozdemir
 */

#ifndef INC_NVSESP_H_
#define INC_NVSESP_H_

#include "main.h"
#include "stm32f4xx_hal.h"
#include <stdlib.h>
#include <string.h>


#define NS_AP_UART_DMA_RX_SIZE   (1024)
#define NS_AP_LOG_COPY_MAX       (1024)


typedef enum {


	NS_ESP_RESET,
    ESP_INIT_OK,

	NS_ESP_STATUS_RESET_OK,
	NS_ESP_STATUS_RESET_FAIL,

	NS_ESP_STATUS_ATE0_FAIL,

	NS_ESP_STATUS_CWMODE_FAIL,

	NS_ESP_STATUS_CWQAP_FAIL,

	NS_ESP_STATUS_WIFI_OK,
	NS_ESP_STATUS_WIFI_FAIL,

	NS_ESP_STATUS_CIPMUX_FAIL,

	NS_ESP_STATUS_CIPSERVER_OK,
	NS_ESP_STATUS_CIPSERVER_FAIL,



	NS_ESP_STATUS_METADATA_FAIL,
	NS_ESP_STATUS_METADATA_SUCCESS,

	NS_ESP_STATUS_SUCCESS,
	NS_ESP_STATUS_FAIL,
	NS_ESP_STATUS_TIMEOUT
} NVS_ESP_STATUS;


typedef enum
{
    ESP_RESET,
    ESP_ATE0,
    ESP_CWMODE,
    ESP_CWSAP,
    ESP_CWQAP,
    ESP_WIFI,
    ESP_CIPMUX,
    ESP_CIPSTART,
    ESP_CIPSEND,
    ESP_HTTP_REQ,
    ESP_SERVER,
    ESP_META_DATA,
    ESP_REQUEST_FIRMWARE,
    ESP_DONE
} NVS_ESP_STATE;

typedef struct
{
    uint16_t httpPayloadLength;
    uint8_t  rxBuffer[2048];
    uint32_t timeout;

    char ATcommand[64];
    char txBuffer[1024];




} NVS_ESP_HANDLE;

typedef struct {
    uint8_t  RxData[NS_AP_UART_DMA_RX_SIZE];
    char     g_log_buf[NS_AP_LOG_COPY_MAX];
    volatile bool g_rx_ready;
    volatile uint16_t g_rx_size;

    char     jsonBuf[256];
    uint8_t rxBuffer[2048];
    char     ssid[32];
    char     password[64];
    bool     ssidPassFound;

    bool     joinPending;
    bool     joinSuccess;
    bool     joinFailed;
    uint32_t joinStartTick;

    char     joinBuf[256];
    uint16_t joinBufLen;

    uint8_t  activeLinkId;
    bool     hasActiveLink;
    bool     internetConnectCheck;
    bool     serverConnectCheck;
    bool     cipsendConnectCheck;
    bool     aaaaa;

} NVS_ESP_AP_MODE;

NVS_ESP_STATUS nvsesp_sendAtCommand(const char *cmd, uint32_t timeoutMs);
NVS_ESP_STATUS nvsesp_initMode(void);
void nvsesp_apModeGetData(void);
void nvsesp_apModeGetDataInit(void);
int nvsesp_parseVersionJson(const char *rxBuffer);
NVS_ESP_STATUS nvsesp_sendApAck(void);
NVS_ESP_STATUS nvsesp_init(void);
extern NVS_ESP_AP_MODE apModeGetData;




#endif /* INC_NVSESP_H_ */
