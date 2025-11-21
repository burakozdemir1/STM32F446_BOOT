/*
 * esp8266_comm.c
 *
 *  Created on: Nov 22, 2025
 *      Author: Burak Ozdemir
 *
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "main.h"
#include "esp8266_comm.h"
#include "nvslogger.h"

unsigned int __StackLimit, __StackTop;
#define FLASH_PROGRAM_TYPE  FLASH_TYPEPROGRAM_WORD


static uint32_t GetSectorFromAddress(uint32_t addr)
{
    if      (addr < 0x08004000) return FLASH_SECTOR_0;  /* 0x08000000 - 0x08003FFF */
    else if (addr < 0x08008000) return FLASH_SECTOR_1;  /* 0x08004000 - 0x08007FFF */
    else if (addr < 0x0800C000) return FLASH_SECTOR_2;  /* 0x08008000 - 0x0800BFFF */
    else if (addr < 0x08010000) return FLASH_SECTOR_3;  /* 0x0800C000 - 0x0800FFFF */
    else if (addr < 0x08020000) return FLASH_SECTOR_4;  /* 0x08010000 - 0x0801FFFF */
    else if (addr < 0x08040000) return FLASH_SECTOR_5;  /* 0x08020000 - 0x0803FFFF */
    else if (addr < 0x08060000) return FLASH_SECTOR_6;  /* 0x08040000 - 0x0805FFFF */
    else if (addr < 0x08080000) return FLASH_SECTOR_7;  /* 0x08060000 - 0x0807FFFF */
    else                        return FLASH_SECTOR_7;  /* Out-of-range: fallback */
}

ESP_Boot ESP8266_GetFirmwareMetadata(uint16_t *blocks)
{
    ESP_Handle_t metaData;
    metaData.timeout = 3000;

    metaData.httpPayloadLength = sprintf(metaData.txBuffer,
        "GET /metadata HTTP/1.1\r\n"
        "Host: burakozdemir1.pythonanywhere.com\r\n"
        "Connection: close\r\n\r\n");

    sprintf(metaData.cipSendCmd, "AT+CIPSEND=0,%d\r\n", metaData.httpPayloadLength);
    HAL_UART_Transmit(&huart1, (uint8_t *)metaData.cipSendCmd,
                      strlen(metaData.cipSendCmd), 1000);
    HAL_Delay(200);

    HAL_UART_Transmit(&huart1, (uint8_t *)metaData.txBuffer,
                      metaData.httpPayloadLength, 1000);

    uint32_t startTime = HAL_GetTick();
    uint16_t index = 0;
    uint8_t ch;

    memset(metaData.rxBuffer, 0, sizeof(metaData.rxBuffer));

    while (HAL_GetTick() - startTime < metaData.timeout &&
           index < sizeof(metaData.rxBuffer) - 1)
    {
        if (HAL_UART_Receive(&huart1, &ch, 1, 100) == HAL_OK)
        {
            metaData.rxBuffer[index++] = ch;
        }
    }
    metaData.rxBuffer[index] = '\0';

    char *ipdStart = strstr((char *)metaData.rxBuffer, "+IPD,0,");
    if (!ipdStart)
    {
        logInfo("BL ERROR: +IPD header not found in metadata response.\n");
        return ESP_STATUS_METADATA_FAIL;
    }

    int ipdLen = 0;
    if (sscanf(ipdStart, "+IPD,0,%d:", &ipdLen) != 1 || ipdLen <= 0)
    {
        logInfo("BL ERROR: IPD length parse failed!\n");
        return ESP_STATUS_METADATA_FAIL;
    }

    char *payloadStart = strchr(ipdStart, ':');
    if (!payloadStart)
    {
        logInfo("BL ERROR: ':' not found after +IPD\n");
        return ESP_STATUS_METADATA_FAIL;
    }

    payloadStart++;
    char *jsonStart = strstr(payloadStart, "{");
    if (!jsonStart)
    {
        logInfo("BL ERROR: JSON start '{' not found!\n");
        return ESP_STATUS_METADATA_FAIL;
    }

    char *countField = strstr(jsonStart, "\"block_count\":");
    if (!countField)
    {
        logInfo("BL ERROR: 'block_count' field not found in JSON!\n");
        return ESP_STATUS_METADATA_FAIL;
    }

    uint16_t blockCount = (uint16_t)atoi(countField + strlen("\"block_count\":"));
    *blocks = blockCount;
    logInfo("BL DEBUG MSG: Parsed block_count = %d\n", blockCount);

    return ESP_STATUS_METADATA_SUCCESS;
}



FlashStatus_t Flash_Write(uint32_t Address, uint8_t *u8Data, uint16_t Size)
{
    FLASH_EraseInitTypeDef EraseInit;
    uint32_t SectorError = 0;
    static uint32_t lastErasedSector = 0xFFFFFFFF;


    if (Address < APP_START_ADDRESS || (Address + Size) > APP_END_ADDRESS)
    {
        logInfo("BL ERROR: Flash_Write out of APP range! addr=0x%08lX size=%u\n",
                Address, Size);
        return FLASH_ERROR;
    }

    HAL_FLASH_Unlock();


    for (uint32_t offset = 0; offset < Size; offset += 4)
    {
        uint32_t currentAddress = Address + offset;
        uint32_t sector = GetSectorFromAddress(currentAddress);

        if (sector != lastErasedSector)
        {
            EraseInit.TypeErase     = FLASH_TYPEERASE_SECTORS;
            EraseInit.Sector        = sector;
            EraseInit.NbSectors     = 1;
            EraseInit.VoltageRange  = FLASH_VOLTAGE_RANGE_3;

            logInfo("BL DEBUG MSG: Flash erase sector=%lu (addr=0x%08lX)\n",
                    (unsigned long)sector, (unsigned long)currentAddress);

            if (HAL_FLASHEx_Erase(&EraseInit, &SectorError) != HAL_OK)
            {
                logInfo("BL ERROR: Flash erase failed! sector=%lu err=0x%08lX\n",
                        (unsigned long)sector, (unsigned long)SectorError);
                HAL_FLASH_Lock();
                return FLASH_ERROR;
            }

            lastErasedSector = sector;
        }
        uint32_t word = 0xFFFFFFFFU;
        uint8_t *dst = (uint8_t*)&word;
        uint32_t remaining = Size - offset;
        uint32_t chunk = (remaining >= 4) ? 4 : remaining;

        for (uint32_t i = 0; i < chunk; i++)
        {
            dst[i] = u8Data[offset + i];
        }

        if (HAL_FLASH_Program(FLASH_PROGRAM_TYPE, currentAddress, word) != HAL_OK)
        {
            logInfo("BL ERROR: Flash program failed at addr=0x%08lX\n",
                    (unsigned long)currentAddress);
            HAL_FLASH_Lock();
            return FLASH_ERROR;
        }
    }

    HAL_FLASH_Lock();
    return FLASH_OK;
}


ESP_Boot ESP8266_RequestFirmware(uint16_t *totalBlocks)
{
    ESP_Handle_t request;
    char getPayload[256];
    request.timeout = 6000;

    for (uint16_t block = 0; block < *totalBlocks; block++)
    {
        uint32_t targetAddress = APP_START_ADDRESS + (block * 512);
        logInfo("BL DEBUG MSG: Handling block %d... target=0x%08lX\n",
                block, (unsigned long)targetAddress);

        sprintf(request.cipSendCmd,
                "AT+CIPSTART=0,\"TCP\",\"burakozdemir1.pythonanywhere.com\",80\r\n");
        HAL_UART_Transmit(&huart1, (uint8_t*)request.cipSendCmd,
                          strlen(request.cipSendCmd), 1000);
        HAL_Delay(500);

        request.httpPayloadLength = sprintf(getPayload,
            "GET /get_block?block=%d HTTP/1.1\r\n"
            "Host: burakozdemir1.pythonanywhere.com\r\n"
            "Connection: close\r\n"
            "\r\n", block);

        sprintf(request.cipSendCmd, "AT+CIPSEND=0,%d\r\n", request.httpPayloadLength);
        HAL_UART_Transmit(&huart1, (uint8_t*)request.cipSendCmd,
                          strlen(request.cipSendCmd), 1000);
        HAL_Delay(200);

        HAL_UART_Transmit(&huart1, (uint8_t*)getPayload,
                          request.httpPayloadLength, 1000);
        logInfo("BL DEBUG MSG: GET sent for block %d\n", block);

        memset(request.rxBuffer, 0, sizeof(request.rxBuffer));
        uint32_t start_time = HAL_GetTick();
        uint16_t index = 0;
        uint8_t ch;

        while (HAL_GetTick() - start_time < request.timeout &&
               index < sizeof(request.rxBuffer) - 1)
        {
            if (HAL_UART_Receive(&huart1, &ch, 1, 50) == HAL_OK)
            {
                request.rxBuffer[index++] = ch;
            }
        }
        request.rxBuffer[index] = '\0';

        char* ipdStart = strstr((char*)request.rxBuffer, "+IPD,0,");
        if (!ipdStart)
        {
            logInfo("BL ERROR: +IPD header not found in block %d!\n", block);
            return ESP_STATUS_METADATA_FAIL;
        }

        int ipdLen = 0;
        if (sscanf(ipdStart, "+IPD,0,%d:", &ipdLen) != 1 ||
            ipdLen <= 0 || ipdLen > 1500)
        {
            logInfo("BL ERROR: Invalid IPD length in block %d!\n", block);
            return ESP_STATUS_METADATA_FAIL;
        }

        char* dataStart = strchr(ipdStart, ':');
        if (!dataStart)
        {
            logInfo("BL ERROR: ':' not found after +IPD in block %d!\n", block);
            return ESP_STATUS_METADATA_FAIL;
        }
        dataStart++;

        char* binDataStart = strstr(dataStart, "\r\n\r\n");
        if (!binDataStart)
        {
            logInfo("BL ERROR: HTTP header end not found in block %d!\n", block);
            return ESP_STATUS_METADATA_FAIL;
        }
        binDataStart += 4;

        int binLen = ipdLen - (int)(binDataStart - dataStart);
        if (binLen < 4 || binLen > 516)
        {
            logInfo("BL DEBUG MSG: Invalid binLen in block %d! binLen=%d\n",
                    block, binLen);
            return ESP_STATUS_METADATA_FAIL;
        }

        uint32_t expectedCRC =
            ((uint32_t)(uint8_t)binDataStart[binLen - 4] << 24) |
            ((uint32_t)(uint8_t)binDataStart[binLen - 3] << 16) |
            ((uint32_t)(uint8_t)binDataStart[binLen - 2] << 8)  |
            ((uint32_t)(uint8_t)binDataStart[binLen - 1] << 0);

        logInfo("BL DEBUG MSG: CRC Block %d - Expected CRC from server: 0x%08lX\n",
                block, (unsigned long)expectedCRC);

        uint32_t calculatedCRC = ESP8266_CalculateCRC(
            (uint8_t*)binDataStart, (uint32_t)(binLen - 4));

        if (expectedCRC != calculatedCRC)
        {
            logInfo("BL ERROR: CRC mismatch at block %d! exp=0x%08lX calc=0x%08lX\n",
                    block, (unsigned long)expectedCRC, (unsigned long)calculatedCRC);
            return ESP_STATUS_METADATA_FAIL;
        }

        logInfo("BL DEBUG MSG: CRC OK for block %d\n", block);

        FlashStatus_t status = Flash_Write(targetAddress,
                                           (uint8_t*)binDataStart,
                                           (uint16_t)(binLen - 4));
        if (status != FLASH_OK)
        {
            logInfo("BL ERROR: Flash write failed at block %d, addr 0x%08lX\n",
                    block, (unsigned long)targetAddress);
            return ESP_STATUS_METADATA_FAIL;
        }

        logInfo("BL DEBUG MSG: Block %d written to 0x%08lX\n",
                block, (unsigned long)targetAddress);
    }

    return ESP_STATUS_METADATA_SUCCESS;
}


uint32_t ESP8266_CalculateCRC(const uint8_t *data, uint32_t length)
{
    uint32_t crc  = 0xFFFFFFFFU;
    uint32_t poly = 0x04C11DB7U;

    for (uint32_t i = 0; i < length; i++)
    {
        crc ^= ((uint32_t)data[i]) << 24;

        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 0x80000000U)
            {
                crc = (crc << 1) ^ poly;
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    crc &= 0xFFFFFFFFU;

    logInfo("BL DEBUG MSG: Calculated CRC over %lu bytes: 0x%08lX\n",
            (unsigned long)length, (unsigned long)crc);

    return crc;
}
