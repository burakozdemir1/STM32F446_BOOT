/*
 * nvsesp8266.c
 *
 *  Created on: Nov 22, 2025
 *      Author: Burak Ozdemir
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdarg.h>
#include "main.h"
#include "esp8266_comm.h"
#include "nvsesp.h"
#include "nvsdatabase.h"
#include "nvslogger.h"

int nvsesp_parseVersionJson(const char *rxBuffer)
{
    if (rxBuffer == NULL) return -1;

    const char *jsonStart = strstr(rxBuffer, "{\"version\":\"");
    if (jsonStart) {
        jsonStart += strlen("{\"version\":\"");

        char versionStr[4] = {0};  // Maks 3 basamaklı versiyon
        int i = 0;
        while (jsonStart[i] && jsonStart[i] != '"' && i < sizeof(versionStr) - 1) {
            versionStr[i] = jsonStart[i];
            i++;
        }
        versionStr[i] = '\0';

        int version = atoi(versionStr);

        char logStr[64];
        sprintf(logStr, "BL INFO: Firmware version = %d\n", version);
        logInfo(logStr);

        return version;
    }

    logInfo("BL ERROR: Version JSON not found.\n");
    return -1;
}


NVS_ESP_STATUS nvsesp_httpRequest(void)
{
	  char httpRequest[256];
	  sprintf(httpRequest,
	      "GET /version HTTP/1.1\r\n"
	      "Host: burakozdemir1.pythonanywhere.com\r\n"
	      "Connection: close\r\n"
	      "\r\n");

	  // AT+CIPSEND ile uzunluğu bildir
	  char cipSendCmd[32];
	  sprintf(cipSendCmd, "AT+CIPSEND=0,%d", strlen(httpRequest));
		ESP_Handle_t espWifi;
		NVS_ESP_STATUS espStatus = NS_ESP_STATUS_FAIL;
		uint8_t ATisOK = 0;
		espWifi.timeout = 6000;

		while (!ATisOK)
		{

		    memset(espWifi.rxBuffer, 0, sizeof(espWifi.rxBuffer));

		    if (HAL_UART_Transmit(&huart1, (uint8_t*)httpRequest, strlen(httpRequest), 2000) != HAL_OK)
		    {
		    	logInfo("BL ERROR: Failed to transmit AT+CIPCLOSE command!\n");
		        break;
		    }

		    uint32_t start_time = HAL_GetTick();
		    while (HAL_UART_Receive(&huart1, (uint8_t*)espWifi.rxBuffer, sizeof(espWifi.rxBuffer), espWifi.timeout) != HAL_OK)
		    {
		        if (HAL_GetTick() - start_time >= espWifi.timeout)
		            break;
		    }

		    if ((strstr((char*)espWifi.rxBuffer, "CLOSED") ||
		         strstr((char*)espWifi.rxBuffer, "OK")))
		    {
		    	logInfo("BL DEBUG: AT+CIPCLOSE OK.\n");
		        espStatus = NS_ESP_STATUS_SUCCESS;
		        ATisOK = 1;
		        break;
		    }
		    else
		    {
		    	logInfo("BL DEBUG: AT+CIPCLOSE empty.\n");
		        break;
		    }

		    HAL_Delay(500);
		}


		if(nvsesp_parseVersionJson((char*)espWifi.rxBuffer)!=12)
		{
			logInfo("BL DEBUG: Updating...\n");
			//nvsesp_sendAtCommand("AT+CIPSTART=0,\"TCP\",\"burakozdemir1.pythonanywhere.com\",80", 2000);
			if(nvsesp_sendAtCommand("AT+CIPSTART=0,\"TCP\",\"burakozdemir1.pythonanywhere.com\",80", 2000)!=NS_ESP_STATUS_SUCCESS)
			{
				logInfo("BL DEBUG MSG: ESP CIPSTART failed!\n");
				espStatus = NS_ESP_STATUS_FAIL;
			}
			else
			{
				logInfo("BL DEBUG MSG: ESP CIPSTART success.\n");
		        espStatus = NS_ESP_STATUS_SUCCESS;
		        ATisOK = 1;
//				ESP8266_GetFirmwareMetadata(&totalBlocks);
//				ESP8266_RequestFirmware(&totalBlocks); // başarılı olması durumunda sistemi resetlemesi için json da bir değişkeni set et ve app koduna geçildiğinde
			}


		}

		ATisOK = 0;
		return espStatus;
}


//NVS_ESP_STATUS nvsesp_init(void)
//{
//	 while (espStatus != ESP_DONE)
//	 {
//		 switch(espStatus)
//		 {
//			 case ESP_RESET :
//				 if (nvsesp_sendAtCommand("AT+RST", 1000) != NS_ESP_STATUS_SUCCESS)
//				 {
//					 printMessage("BL ERROR: ESP Reset failed!\n");
//					 espStatus = ESP_DONE ;
//				 }
//
//				 else{
//					 printMessage("BL DEBUG MSG: ESP Reset Success.\n");
//					 espStatus = ESP_ATE0 ;
//				 }
//
//					 break;
//
//			 case ESP_ATE0:
//
//				 if (nvsesp_sendAtCommand("ATE0", 1000) != NS_ESP_STATUS_SUCCESS)
//				 {
//					 printMessage("BL DEBUG MSG: ESP ATE0 failed!\n");
//					 espStatus = ESP_DONE ;
//				 }
//
//
//				 else{
//					 printMessage("BL DEBUG MSG: ESP ATE0 success.\n");
//					 espStatus = ESP_CWMODE;
//				 }
//
//					 break;
//
//			 case ESP_CWMODE:
//
//				 if (nvsesp_sendAtCommand("AT+CWMODE=3", 1000) != NS_ESP_STATUS_SUCCESS)
//				 {
//					 printMessage("BL ERROR: Failed to CWMODE.\n");
//					 espStatus = ESP_DONE ;
//					 break;
//				 }
//
//				 else {
//					 printMessage("BL DEBUG MSG: CWMODE success.\n");
//					 espStatus = ESP_CWSAP ;
//				 }
//
//
//				 break;
//
//			 case ESP_CWSAP:
//					if(nvsesp_sendAtCommand("AT+CWSAP=\"BURAK123\",\"12345678\",5,3", 2000)!=NS_ESP_STATUS_SUCCESS)
//					{
//						printMessage("BL ERROR: Failed to CWSAP.\n");
//						espStatus = ESP_DONE ;
//						break;
//					}
//					 else{
//						printMessage("BL DEBUG MSG: CWSAP success.\n");
//						espStatus = ESP_CWQAP ;
//					 }
//
//
//					 break;
//
//			 case ESP_CWQAP:
//					 if(nvsesp_sendAtCommand("AT+CWQAP", 1000)!=NS_ESP_STATUS_SUCCESS)
//					 {
//						printMessage("BL ERROR: Failed to CWQAP.\n");
//						espStatus = ESP_DONE ;
//						break;
//					 }
//					 else
//					 {
//						//printMessage("BL DEBUG MSG: Total Blocks %d\n",totalBlocks);
//						printMessage("BL DEBUG MSG: CWQAP success.\n");
//						espStatus = ESP_WIFI ;
//					 }
//
//					 break;
//
//			 case ESP_WIFI:
//					 if(nvsesp_sendAtCommand("AT+CWJAP=\"" WIFI_SSID "\",\"" WIFI_PASSWORD "\"", 3000)!=NS_ESP_STATUS_SUCCESS)
//					 {
//						printMessage("BL ERROR: Failed to WIFI.\n");
//						espStatus = ESP_DONE ;
//						break;
//					 }
//					 else
//					 {
//						 printMessage("BL DEBUG MSG: WIFI success.\n");
//						 espStatus = ESP_CIPMUX ;
//					 }
//				 break;
//			 case ESP_CIPMUX:
//				 if(nvsesp_sendAtCommand("AT+CIPMUX=1", 4000)!=NS_ESP_STATUS_SUCCESS)
//				 {
//						printMessage("BL ERROR: Failed to CIPMUX.\n");
//						espStatus = ESP_DONE ;
//
//				 }
//				 else
//				 {
//					 printMessage("BL DEBUG MSG: CIPMUX success.\n");
//					 espStatus = ESP_CIPSTART;
//				 }
//
//				 break;
//
//			 case ESP_CIPSTART:
//				 if(nvsesp_sendAtCommand("AT+CIPSTART=0,\"TCP\",\"burakozdemir1.pythonanywhere.com\",80", 4000)!=NS_ESP_STATUS_SUCCESS)
//				 {
//						printMessage("BL ERROR: Failed to CIPSTART.\n");
//						espStatus = ESP_DONE ;
//
//				 }
//				 else
//				 {
//					 printMessage("BL DEBUG MSG: CIPSTART success.\n");
//					 espStatus = ESP_CIPSEND;
//				 }
//				 break;
//			 case ESP_CIPSEND:
//				  char httpRequest[256];
//				  sprintf(httpRequest,
//				      "GET /version HTTP/1.1\r\n"
//				      "Host: burakozdemir1.pythonanywhere.com\r\n"
//				      "Connection: close\r\n"
//				      "\r\n");
//
//				  // AT+CIPSEND ile uzunluğu bildir
//				  char cipSendCmd[32];
//				  sprintf(cipSendCmd, "AT+CIPSEND=0,%d", strlen(httpRequest));
//
//
//			    if(nvsesp_sendAtCommand(cipSendCmd, 4000)!=NS_ESP_STATUS_SUCCESS)
//				{
//					printMessage("BL ERROR: Failed to CIPSEND.\n");
//					espStatus = ESP_DONE ;
//
//				}
//			    else
//			    {
//					 printMessage("BL DEBUG MSG: CIPSEND success.\n");
//					 espStatus = ESP_HTTP_REQ;
//			    }
//
//				 break; // nvsesp_httpRequest
//			 case ESP_HTTP_REQ:
//				    if(nvsesp_httpRequest()!=NS_ESP_STATUS_SUCCESS)
//					{
//						printMessage("BL ERROR: Failed to ESP_HTTP_REQ.\n");
//						espStatus = ESP_DONE ;
//
//					}
//				    else
//				    {
//						 printMessage("BL DEBUG MSG: ESP_HTTP_REQ success.\n");
//						 espStatus = ESP_SERVER;
//				    }
//
//				 break;
//			 case ESP_SERVER:
//				    if(nvsesp_httpRequest()!=NS_ESP_STATUS_SUCCESS)
//					{
//						printMessage("BL ERROR: Failed to SERVER.\n");
//						espStatus = ESP_DONE ;
//
//					}
//				    else
//				    {
//						 printMessage("BL DEBUG MSG: SERVER success.\n");
//						 ESP8266_GetFirmwareMetadata(&totalBlocks);
//						 espStatus = ESP_DONE;
//				    }
//
//				 break;
//
//
//
//			 default:
//				espStatus = ESP_DONE ;
//				 break;
//		 }
//	 }
//}



NVS_ESP_STATUS nvsesp_sendAtCommand(const char *cmd, uint32_t timeoutMs)
{
	NVS_ESP_HANDLE espHandle;
	NVS_ESP_STATUS espStatus = NS_ESP_STATUS_FAIL;
	uint8_t ATisOK = 0;
	uint8_t failCount = 0; // Eklenen sayaç
	espHandle.timeout = timeoutMs;
	memset(espHandle.ATcommand, 0, sizeof(espHandle.ATcommand));

	while (!ATisOK) {
		// Eğer 5 denemeden fazla olduysa çık
		if (failCount >= 5) {
			logInfo("BL ERROR: Too many failed attempts, exiting...\n");
			break;
		}

		sprintf(espHandle.ATcommand, "%s\r\n", cmd);
		memset(espHandle.rxBuffer, 0, sizeof(espHandle.rxBuffer));

		if (HAL_UART_Transmit(&huart1, (uint8_t*)espHandle.ATcommand, strlen(espHandle.ATcommand), 1000) != HAL_OK)
		{
			char msg[160];
			snprintf(msg, sizeof(msg), "BL ERROR: Transmit failed for cmd: %s\n", cmd);
			logInfo(msg);
			break;
		}

		uint32_t start_time = HAL_GetTick();
		while (HAL_UART_Receive(&huart1, (uint8_t*)espHandle.rxBuffer, sizeof(espHandle.rxBuffer), espHandle.timeout) != HAL_OK)
		{
			if (HAL_GetTick() - start_time >= espHandle.timeout)
				break;
		}

		if (strstr((char*)espHandle.rxBuffer, ">") || strstr((char*)espHandle.rxBuffer, "CLOSED") ||
			strstr((char*)espHandle.rxBuffer, "OK") || strstr((char*)espHandle.rxBuffer, "ready"))
		{
			char msg[160];
			snprintf(msg, sizeof(msg), "BL INFO: SUCCESS for cmd: %s\n", cmd);
			logInfo(msg);
			ATisOK = 1;
			espStatus = NS_ESP_STATUS_SUCCESS;
		}
		else
		{
			char msg[160];
			snprintf(msg, sizeof(msg), "BL ERROR: Transmit failed for cmd: %s\n", cmd);
			logInfo(msg);
			failCount++; // Başarısız deneme sayısını artır
		}
	}

	return espStatus;
}





void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
	/*ISR Fonksiyonlarında Log yapma!!!*/
    if (huart->Instance == USART1)
    {
        uint16_t n = (Size < NS_AP_LOG_COPY_MAX) ? Size : NS_AP_LOG_COPY_MAX;

        memcpy((void*)apModeGetData.g_log_buf,apModeGetData.RxData, n);
        apModeGetData.g_rx_size  = n;
        apModeGetData.g_rx_ready = true;

        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, apModeGetData.RxData, NS_AP_UART_DMA_RX_SIZE);
        __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
    }
}




/* Gelen +IPD/HTTP verisinden JSON gövdeyi çıkarır.
 * data,len: UART’tan alınan ham chunk
 * jsonOut: JSON metnini yazacağımız buffer
 * jsonOutSize: jsonOut kapasitesi
 * Dönüş: true -> JSON başarıyla kopyalandı, false -> bulunamadı/eksik/taştı
 *
 * Örnek beklenen çıktı: {"ssid":"burakozdemir55","password":"854321"}
 */
bool nvsesp_extractJsonBody(const uint8_t *data, uint16_t len, char *jsonOut, uint16_t jsonOutSize)
{
    if (!data || !jsonOut || jsonOutSize < 3) return false;

    /* 1) HTTP header sonunu bul: "\r\n\r\n" */
    int hdrEnd = -1;
    for (uint16_t i = 0; i + 3 < len; i++) {
        if (data[i] == '\r' && data[i+1] == '\n' && data[i+2] == '\r' && data[i+3] == '\n') {
            hdrEnd = (int)i + 4;
            break;
        }
    }

    /* 2) Header varsa Content-Length'i header içinde ara (case-insensitive) */
    int contentLen = -1;
    if (hdrEnd > 0) {
        const char *needle = "content-length:";
        int nlen = 15; /* "content-length:" */
        for (int i = 0; i + nlen <= hdrEnd; i++) {
            int k = 0;
            while (k < nlen) {
                char a = (char)data[i+k];
                char b = needle[k];
                if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
                if (a != b) break;
                k++;
            }
            if (k == nlen) {
                /* Sayıya kadar boşlukları atla */
                int p = i + nlen;
                while (p < hdrEnd && (data[p] == ' ' || data[p] == '\t')) p++;
                /* Sayıyı oku */
                int v = 0, ok = 0;
                while (p < hdrEnd && data[p] >= '0' && data[p] <= '9') {
                    ok = 1; v = v*10 + (data[p]-'0'); p++;
                }
                if (ok) contentLen = v;
                break;
            }
        }
    }

    /* 3) Content-Length varsa gövdeyi tam kopyala */
    if (hdrEnd > 0 && contentLen >= 0) {
        int bodyAvail = (int)len - hdrEnd;
        if (bodyAvail >= contentLen && contentLen < (int)jsonOutSize) {
            memcpy(jsonOut, &data[hdrEnd], (size_t)contentLen);
            jsonOut[contentLen] = '\0';
            return true;
        }
        /* Eksik veya sığmıyor: fallback'e geç */
    }

    /* 4) Fallback: chunk içinde JSON'ı { ... } aralığıyla yakala */
    int start = -1, stop = -1;
    for (uint16_t i = 0; i < len; i++) {
        if (data[i] == '{') { start = (int)i; break; }
    }
    if (start >= 0) {
        for (int i = (int)len - 1; i > start; i--) {
            if (data[i] == '}') { stop = i; break; }
        }
    }
    if (start >= 0 && stop > start) {
        int jsonLen = stop - start + 1;
        if (jsonLen < (int)jsonOutSize) {
            memcpy(jsonOut, &data[start], (size_t)jsonLen);
            jsonOut[jsonLen] = '\0';
            return true;
        }
    }

    /* Bulunamadı */
    if (jsonOutSize) jsonOut[0] = '\0';
    return false;
}
bool nvsesp_jsonFindString(const char *json, const char *key, char *out, size_t outlen) {
    char pattern[32];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    const char *start = strstr(json, pattern);
    if (!start) return false;
    start += strlen(pattern);
    const char *end = strchr(start, '"');
    if (!end) return false;
    size_t len = end - start;
    if (len >= outlen) len = outlen-1;
    memcpy(out, start, len);
    out[len] = '\0';
    return true;
}
static void nvsesp_joinFeed(const char* chunk, uint16_t len) {
    if (!apModeGetData.joinPending || len == 0) return;

    // Chunk’ı bounded şekilde joinBuf’a ekle
    uint16_t space = sizeof(apModeGetData.joinBuf) - 1 - apModeGetData.joinBufLen;
    if (space > 0) {
        uint16_t copy = (len < space) ? len : space;
        memcpy(&apModeGetData.joinBuf[apModeGetData.joinBufLen], chunk, copy);
        apModeGetData.joinBufLen += copy;
        apModeGetData.joinBuf[apModeGetData.joinBufLen] = '\0';
    }

    // Anahtar kelimeleri ara
    const char* s = apModeGetData.joinBuf;

    // Başarı işaretleri
    bool hasConnected = (strstr(s, "WIFI CONNECTED") != NULL);
    bool hasGotIp     = (strstr(s, "WIFI GOT IP")   != NULL) || (strstr(s, "GOT IP") != NULL);
    bool hasOk        = (strstr(s, "\r\nOK\r\n")    != NULL) || (strstr(s, "\nOK\n") != NULL) || (strstr(s, "OK\r\n") != NULL);

    // Hata işaretleri
    bool hasFail      = (strstr(s, "\r\nFAIL\r\n")  != NULL) || (strstr(s, "FAIL") != NULL);
    bool hasError     = (strstr(s, "\r\nERROR\r\n") != NULL) || (strstr(s, "ERROR") != NULL);

    // +CWJAP:x hata kodu (1..6 gibi)
    bool hasCwjapErr  = (strstr(s, "+CWJAP:") != NULL);

    // Karar
    if (hasGotIp || (hasConnected && hasOk)) {
        apModeGetData.joinSuccess = true;
        apModeGetData.joinPending = false;
    } else if (hasFail || hasError || hasCwjapErr) {
        apModeGetData.joinFailed  = true;
        apModeGetData.joinPending = false;
    } else {
        // Hâlâ bekliyoruz; timeout kontrolünü dışarıda yapacağız.
    }
}
void nvsesp_apModeGetData(void)
{
    if (apModeGetData.g_rx_ready) {
        __disable_irq();
        uint16_t sz = apModeGetData.g_rx_size;
        apModeGetData.g_rx_ready = false;
        __enable_irq();

        apModeGetData.ssidPassFound = false; // Her çağrıda önce sıfırla

        if (nvsesp_extractJsonBody(apModeGetData.g_log_buf, sz, apModeGetData.jsonBuf, sizeof(apModeGetData.jsonBuf))) {
        	logInfo("Extracted JSON:\n");
        	logInfo(apModeGetData.jsonBuf);
        	logInfo("\n");

            bool foundSsid = nvsesp_jsonFindString(apModeGetData.jsonBuf, "ssid", apModeGetData.ssid, sizeof(apModeGetData.ssid));
            bool foundPassword = nvsesp_jsonFindString(apModeGetData.jsonBuf, "password", apModeGetData.password, sizeof(apModeGetData.password));

            if (foundSsid) {
            	logInfo("SSID found: ");
            	logInfo(apModeGetData.ssid);
            	logInfo("\n");
            } else {
            	logInfo("No SSID found!\n");
            }

            if (foundPassword) {
            	logInfo("Password found: ");
            	logInfo(apModeGetData.password);
            	logInfo("\n");
            } else {
            	logInfo("No password found!\n");
            }

            // ----> İki veri de geldiyse
            if (foundSsid && foundPassword) {
                apModeGetData.ssidPassFound = true;
            }

            databaseHandle.databaseApModeGetJSON = true;
        } else {
        	logInfo("No JSON found in this chunk.\n");
        }
        nvsesp_joinFeed(apModeGetData.g_log_buf, sz);
    }

}


void nvsesp_apModeGetDataInit(void)
{
	  apModeGetData.g_rx_ready = false;
	  HAL_UARTEx_ReceiveToIdle_DMA(&huart1, apModeGetData.RxData, NS_AP_UART_DMA_RX_SIZE);
	  __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
}
// Ekrana okunur ASCII (printable) göster: okunmayanları '.' yap
void log_ascii_chunked(const uint8_t *data, uint16_t len)
{
    char line[80];  // tek çağrıda en fazla ~78 karakter bas
    int pos = 0;

    for (uint16_t i = 0; i < len; i++) {
        char c = (data[i] >= 32 && data[i] <= 126) ? (char)data[i] : '.';
        line[pos++] = c;
        if (pos >= 78) { // satırı kır
            line[pos++] = '\n';
            line[pos] = 0;
            logInfo(line);
            pos = 0;
        }
    }
    if (pos) {
        line[pos++] = '\n';
        line[pos] = 0;
        logInfo(line);
    }
}

void log_hex_chunked(const uint8_t *data, uint16_t len)
{
    char line[80];
    uint16_t i = 0;

    while (i < len) {
        int pos = 0;
        uint16_t chunk = (len - i > 16) ? 16 : (len - i);
        for (uint16_t j = 0; j < chunk; j++) {
            pos += snprintf(&line[pos], sizeof(line) - pos, "%02X ", data[i + j]);
            if (pos >= (int)sizeof(line) - 4) break;
        }
        line[pos++] = '\n';
        line[pos] = 0;
        logInfo(line);
        i += chunk;
    }
}


