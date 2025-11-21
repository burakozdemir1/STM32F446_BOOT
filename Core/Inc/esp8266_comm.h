/*
 * esp8266_comm.h
 *
 *  Created on: Nov 22, 2025
 *      Author: Burak Ozdemir
 */

#ifndef INC_ESP8266_COMM_H_
#define INC_ESP8266_COMM_H_

#include "main.h"
#include "stm32f4xx_hal.h"
#include <stdlib.h>
#include <string.h>

#define WIFI_SSID     "TP-Link_E7D0"
#define WIFI_PASS     "52227929"

#define APP_START_ADDRESS   0x08020000U    /* Sector 5 başlangıcı */
#define APP_END_ADDRESS     0x08060000U    /* Sector 6 sonu + 1 (exclusive) */
typedef enum {
    FLASH_OK = 0,
    FLASH_ERROR,
    FLASH_OUT_OF_RANGE
} FlashStatus_t;



FlashStatus_t Flash_Write(uint32_t Address, uint8_t *u8Data, uint16_t Size);
uint32_t ESP8266_CalculateCRC(const uint8_t *data, uint32_t length);
#define HTTP_HOST  "burakozdemir1.pythonanywhere.com"
#define HTTP_PORT  80

typedef enum {
	ESP8266_INIT_OK,

	ESP_STATUS_RESET_OK,
    ESP_STATUS_RESET_FAIL,

    ESP_STATUS_ATE0_FAIL,

    ESP_STATUS_CWMODE_FAIL,

    ESP_STATUS_CWQAP_FAIL,

	ESP_STATUS_WIFI_CONNECT_OK,
    ESP_STATUS_WIFI_CONNECT_FAIL,

	ESP_STATUS_CIPMUX_FAIL,

	ESP_STATUS_SERVER_CONNECT_OK,
    ESP_STATUS_SERVER_CONNECT_FAIL,


	ESP_STATUS_METADATA_FAIL,
	ESP_STATUS_METADATA_SUCCESS,


    ESP_STATUS_UART_FAIL,
    ESP_STATUS_TIMEOUT
} ESP_Boot;

typedef enum
{
	ESP8266_RESET,
	ESP8266_CALIB,
	ESP8266_WIFI,
	ESP8266_SERVER,
	ESP8266_META_DATA,
	ESP8266_REQUEST_FIRMWARE,
	ESP8266_DONE
}ESP_Status;

typedef enum
{
	MQTT_CMD_CONNECT,
	MQTT_CMD_SUBSCRIBE,
	MQTT_CMD_PUBLISH
} MQTT_Command_t;


typedef struct
{
	uint16_t httpPayloadLength;
	uint8_t rxBuffer[2048];
	uint32_t timeout ;
	char ATcommand[64];
	char txBuffer[1024];
    char cipSendCmd[64];
}ESP_Handle_t;


ESP_Boot MQTT_ConnectToBroker(char *Ip ,char *Port);
ESP_Boot ESP8266_Reset(void);
ESP_Boot ESP8266_Calibration(void);
ESP_Boot ESP8266_WiFiConnection(void);
ESP_Boot ESP8266_ServerConnection(void);
ESP_Boot ESP8266_GetFirmwareMetadata(uint16_t *blocks);
ESP_Boot MQTT_Subscribe(char *topic);
ESP_Boot MQTT_Publish(char *topic, char *message);


ESP_Boot ESP8266_RequestFirmware(uint16_t *totalBlocks);





#endif /* INC_ESP8266_COMM_H_ */
