/*
 * nvsdatabase.c
 *
 *  Created on: Aug 16, 2025
 *      Author: Burak Ozdemir
 */


#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdarg.h>
#include "main.h"

#include "nvsdatabase.h"
#include "nvsesp.h"
#include "nvslogger.h"
#define FLASH_WRITE_SUCCESS 0
#define FLASH_WRITE_ERROR   1

static NVS_DB_STATUS nvsdatabase_flashWriteCharData(uint32_t address, const char *data)
{
    if (data == NULL || strlen(data) == 0) {
        return NS_DB_STATUS_FAIL;
    }

    if ((address & 0x1u) != 0u) {
        return NS_DB_STATUS_FAIL;
    }

    uint32_t length = (uint32_t)strlen(data);


    if (HAL_FLASH_Unlock() != HAL_OK) {
        return NS_DB_STATUS_FAIL;
    }

    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0;

    if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK) {
        (void)HAL_FLASH_Lock();
        return NS_DB_STATUS_WRITE_FAIL;
    }

    for (uint32_t i = 0; i < length; i += 2u)
    {
        uint32_t dst = address + i;

        uint16_t half_word = (uint8_t)data[i];
        if ((i + 1u) < length) {
            half_word |= (uint16_t)((uint8_t)data[i + 1u] << 8);
        } else {
            half_word |= (uint16_t)(0xFFu << 8);
        }

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, dst, half_word) != HAL_OK)
        {
            (void)HAL_FLASH_Lock();
            return NS_DB_STATUS_WRITE_FAIL;
        }

        if (*(volatile uint16_t *)dst != half_word)
        {
            (void)HAL_FLASH_Lock();
            return NS_DB_STATUS_FAIL;
        }
    }
    	uint16_t term_hw = 0x00FF;
       uint32_t term_addr = address + length;

       if ((term_addr & 0x1u) != 0u) {
           term_addr++;
       }

       if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, term_addr, term_hw) != HAL_OK) {
           (void)HAL_FLASH_Lock();
           return NS_DB_STATUS_WRITE_FAIL;
       }

       if (*(volatile uint16_t *)term_addr != term_hw) {
           (void)HAL_FLASH_Lock();
           return NS_DB_STATUS_FAIL;
       }
    if (HAL_FLASH_Lock() != HAL_OK) {
        return NS_DB_STATUS_FAIL;
    }

    return NS_DB_STATUS_WRITE_OK;
}

NVS_DB_STATUS nvsdatabase_flashReadCharData(uint32_t address, char *buffer, uint32_t maxLength)
{
    if (buffer == NULL || maxLength == 0) {
        return NS_DB_STATUS_READ_FAIL;
    }
    if ((address & 0x1u) != 0u) {
        return NS_DB_STATUS_READ_FAIL;
    }

    uint32_t i = 0;
    uint32_t len_read = 0;
    bool done = false;

    while (i < (maxLength - 1))
    {
        uint16_t half_word = *(volatile uint16_t *)(address + i);

        buffer[i]     = (char)(half_word & 0xFF);
        buffer[i + 1] = (char)((half_word >> 8) & 0xFF);

        if (buffer[i] == '\0') {
            len_read = i;
            done = true;
            break;
        }
        if (buffer[i + 1] == '\0') {
            len_read = i + 1;
            done = true;
            break;
        }

        i += 2;
    }

    if (!done) {
        buffer[maxLength - 1] = '\0';
        len_read = (uint32_t)strnlen(buffer, maxLength - 1);
    }

    {
        char dbg[NS_DB_PASS_MAX + 1] = {0};
        uint32_t copy_len = len_read;
        if (copy_len > NS_DB_PASS_MAX) copy_len = NS_DB_PASS_MAX;

        memcpy(dbg, buffer, copy_len);
        dbg[copy_len] = '\0';

        for (uint32_t k = 0; k < copy_len; ++k) {
            unsigned char c = (unsigned char)dbg[k];
            if (c < 0x20 || c > 0x7E) dbg[k] = '.';
        }

        logInfo("DB READ: addr=0x%08lX len=%lu data='%s'\n",
                     (unsigned long)address, (unsigned long)len_read, dbg);
    }

    return NS_DB_STATUS_READ_OK;
}


static bool nvsesp_jsonFindString(const char *json, const char *key, char *out, size_t maxLen)
{
    if (!json || !key || !out || maxLen == 0) return false;

    char pat[32];
    int n = snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    if (n <= 0 || (size_t)n >= sizeof(pat)) return false;

    const char *p = strstr(json, pat);
    if (!p) return false;

    p += strlen(pat);

    size_t i = 0;
    while (*p && *p != '"' && i + 1 < maxLen) {
        out[i++] = *p++;
    }
    out[i] = '\0';
    if (*p != '"') return false;

    return (i > 0);
}

static bool nvsesp_parseJsonCredentials(const char *json,
                                        char *ssidOut, size_t ssidMax,
                                        char *passOut, size_t passMax)
{
    if (!json || !ssidOut || !passOut) return false;

    char ssid[NS_DB_SSID_MAX + 1] = {0};
    char pass[NS_DB_PASS_MAX + 1] = {0};

    if (!nvsesp_jsonFindString(json, "ssid", ssid, sizeof(ssid))) return false;
    if (!nvsesp_jsonFindString(json, "password", pass, sizeof(pass))) return false;

    strncpy(ssidOut, ssid, ssidMax - 1);  ssidOut[ssidMax - 1] = '\0';
    strncpy(passOut, pass, passMax - 1);  passOut[passMax - 1] = '\0';

    return true;
}

NVS_DB_STATUS nvsdatabase_validate(void)
{
    if (!databaseHandle.databaseApModeGetJSON)
        return NS_DB_STATUS_FAIL;

    __disable_irq();
    databaseHandle.databaseApModeGetJSON = false;
    __enable_irq();

    char ssid[NS_DB_SSID_MAX + 1] = {0};
    char pass[NS_DB_PASS_MAX + 1] = {0};

    if (nvsesp_parseJsonCredentials(apModeGetData.jsonBuf,
                                    ssid, sizeof(ssid),
                                    pass, sizeof(pass)))
    {
        strncpy(databaseHandle.ssid, ssid, sizeof(databaseHandle.ssid) - 1);
        strncpy(databaseHandle.pass, pass, sizeof(databaseHandle.pass) - 1);

        databaseHandle.credentialsReady = true;

        logInfo("DB INFO: Parsed credentials (ssid='%s', pass_len=%u)\n",
                     databaseHandle.ssid, (unsigned)strlen(databaseHandle.pass));

        NVS_DB_STATUS ws = nvsdatabase_flashWriteCharData(NS_DB_FLASH_SSID_ADDR, databaseHandle.ssid);
        NVS_DB_STATUS wp = nvsdatabase_flashWriteCharData(NS_DB_FLASH_PASS_ADDR, databaseHandle.pass);

        if (ws == NS_DB_STATUS_WRITE_OK && wp == NS_DB_STATUS_WRITE_OK) {
            char ssid_verify[NS_DB_SSID_MAX + 1] = {0};
            char pass_verify[NS_DB_PASS_MAX + 1] = {0};
            __disable_irq();
            NVS_DB_STATUS rs = nvsdatabase_flashReadCharData(NS_DB_FLASH_SSID_ADDR, ssid_verify, sizeof(ssid_verify));
            NVS_DB_STATUS rp = nvsdatabase_flashReadCharData(NS_DB_FLASH_PASS_ADDR, pass_verify, sizeof(pass_verify));
            __enable_irq();

            if (rs == NS_DB_STATUS_READ_OK &&
                rp == NS_DB_STATUS_READ_OK &&
                strstr(ssid_verify, databaseHandle.ssid) != NULL &&
                strstr(pass_verify, databaseHandle.pass) != NULL)
            {
            	logInfo("DB SUCCESS: Credentials verified in flash.\n");
                return NS_DB_STATUS_SUCCESS;
            }
            else
            {
            	logInfo("DB ERROR: Credentials verification failed or DB read error.\n");
                return NS_DB_STATUS_FAIL;
            }
        }
        else {
        	logInfo("DB ERROR: Flash write failed (ssid=%d, pass=%d)\n", ws, wp);
            return NS_DB_STATUS_FAIL;
        }
    }
    else
    {
        databaseHandle.credentialsReady = false;
        logInfo("DB ERROR: JSON parse failed (ssid/password not found)\n");
        return NS_DB_STATUS_FAIL;
    }
}

