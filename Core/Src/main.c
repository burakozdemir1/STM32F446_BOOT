/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "nvslogger.h"
#include "nvscfg_store.h"
#include "nvsesp.h"
#include "esp8266_comm.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
uint32_t server_version;
uint8_t g_firmwareVersion = 0;
NVS_ESP_AP_MODE apModeGetData;

uint16_t totalBlocks;
static volatile uint32_t resetValue;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CRC_HandleTypeDef hcrc;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart1_rx;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_CRC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void bootloader_jump_to_user_application(void) {
	void (*bootloader_application_reset_handler)(void);
	logInfo("BL DEBUG MSG:Jumping to user application...\n");
	uint32_t mspValue = *(volatile uint32_t*) APP_START_ADDRESS;
	logInfo("BL DEBUG MSG:MSP Value %#x\n", mspValue);
	__set_MSP(mspValue);

	resetValue = *(volatile uint32_t*) (APP_START_ADDRESS + 4);
	logInfo("BL DEBUG MSG:Reset Value %#x\n", resetValue);

	bootloader_application_reset_handler = (void*) resetValue;

	bootloader_application_reset_handler();

}
void app_test_read_wifi_from_cfg(void)
{
    logInfo("==== APP TEST: READ WIFI CFG START ====\r\n");

    nv_cfg_result_t r = nv_cfg_load_globals();
    logInfo("APP READ: nv_cfg_load_globals returned %d\r\n", r);

    if (r != NV_CFG_OK) {
        logInfo("APP READ ERROR: load_globals FAILED (%d)\r\n", r);
        logInfo("==== APP TEST: READ WIFI CFG END (FAIL) ====\r\n\r\n");
        return;
    }

    logInfo("APP READ: g_nv_cfg.pass_len = %u\r\n", (unsigned)g_nv_cfg.pass_len);
    logInfo("APP READ: g_nv_cfg.ssid     = \"%s\"\r\n", g_nv_cfg.ssid);

    char pass_plain[NV_PASS_MAX_LEN + 1];
    uint16_t pl = g_nv_cfg.pass_len;
    if (pl > NV_PASS_MAX_LEN) pl = NV_PASS_MAX_LEN;
    memcpy(pass_plain, g_nv_cfg.pass, pl);
    pass_plain[pl] = '\0';

    logInfo("APP READ: g_nv_cfg.pass     = \"%s\"\r\n", pass_plain);

    // Maskeleme isteğe bağlı
    char pass_mask[32];
    if (g_nv_cfg.pass_len == 0) {
        strcpy(pass_mask, "(empty)");
    } else if (g_nv_cfg.pass_len >= 4) {
        pass_mask[0] = g_nv_cfg.pass[0];
        pass_mask[1] = g_nv_cfg.pass[1];
        pass_mask[2] = '*';
        pass_mask[3] = '*';
        pass_mask[4] = '*';
        pass_mask[5] = g_nv_cfg.pass[g_nv_cfg.pass_len - 2];
        pass_mask[6] = g_nv_cfg.pass[g_nv_cfg.pass_len - 1];
        pass_mask[7] = '\0';
    } else {
        uint16_t m = (g_nv_cfg.pass_len < sizeof(pass_mask) - 1)
                     ? g_nv_cfg.pass_len
                     : (sizeof(pass_mask) - 1);
        for (uint16_t i = 0; i < m; i++) pass_mask[i] = '*';
        pass_mask[m] = '\0';
    }

    logInfo("APP READ: g_nv_cfg.pass(mask)= %s\r\n", pass_mask);

    logInfo("==== APP TEST: READ WIFI CFG END (OK) ====\r\n\r\n");
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_USART1_UART_Init();
  MX_CRC_Init();
  /* USER CODE BEGIN 2 */
	nvsesp_sendAtCommand("AT+RST", 1000);
	nvsesp_sendAtCommand("ATE0", 1000);
	nvsesp_sendAtCommand("AT+CWMODE=3", 1000);
	nvsesp_sendAtCommand("AT+CWSAP=\"BURAK123\",\"12345678\",5,3", 2000);
	nvsesp_sendAtCommand("AT+CWQAP", 1000);
	char cmd[128];
	logInfo("WiFi join OK (%s)\r\n", g_nv_cfg.ssid);
	snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", g_nv_cfg.ssid, g_nv_cfg.pass);

	if (nvsesp_sendAtCommand(cmd,3000) == NS_ESP_STATUS_SUCCESS) {
		logInfo("wifi success\n");
		nvsesp_sendAtCommand("AT+CIPMUX=1", 1000);

		if (nvsesp_sendAtCommand(
				"AT+CIPSTART=0,\"TCP\",\"burakozdemir1.pythonanywhere.com\",80",
				2000) == NS_ESP_STATUS_SUCCESS) {
			char httpRequest[256];
			sprintf(httpRequest, "GET /version HTTP/1.1\r\n"
					"Host: burakozdemir1.pythonanywhere.com\r\n"
					"Connection: close\r\n"
					"\r\n");

			char cipSendCmd[32];
			sprintf(cipSendCmd, "AT+CIPSEND=0,%d", strlen(httpRequest));
			nvsesp_sendAtCommand(cipSendCmd, 2000);


			ESP_Handle_t espWifi;

			uint8_t ATisOK = 0;
			espWifi.timeout = 6000;

			while (!ATisOK) {

				memset(espWifi.rxBuffer, 0, sizeof(espWifi.rxBuffer));

				if (HAL_UART_Transmit(&huart1, (uint8_t*) httpRequest,
						strlen(httpRequest), 2000) != HAL_OK) {
					logInfo(
							"BL ERROR: Failed to transmit AT+CIPCLOSE command!\n");
					break;
				}

				uint32_t start_time = HAL_GetTick();
				while (HAL_UART_Receive(&huart1, (uint8_t*) espWifi.rxBuffer,
						sizeof(espWifi.rxBuffer), espWifi.timeout) != HAL_OK) {
					if (HAL_GetTick() - start_time >= espWifi.timeout)
						break;
				}

				if ((strstr((char*) espWifi.rxBuffer, "CLOSED")
						|| strstr((char*) espWifi.rxBuffer, "OK"))) {
					logInfo("BL DEBUG: AT+CIPCLOSE OK.\n");
					ATisOK = 1;
					break;
				} else {
					logInfo("BL DEBUG: AT+CIPCLOSE empty.\n");
					break;
				}

				HAL_Delay(500);
			}

			ATisOK = 0;

	        if (!g_nv_cfg.first_online_flag) {
	            /* HTTP yanıt imzası gördüysek “internete bağlandı�? sayıyoruz */
	            if (strstr((char*)espWifi.rxBuffer, "HTTP/1.1") ||
	                strstr((char*)espWifi.rxBuffer, "HTTP/1.0")) {
	                if (nv_cfg_set_first_online_flag_and_commit(true) == NV_CFG_OK) {
	                	logInfo("First online flag set to 1\r\n");
	                } else {
	                	logInfo("First online flag commit FAILED\r\n");
	                }
	            } else {
	                /* Wi-Fi + TCP oldu ama HTTP görmedikse daha katı davranmak istersen
	                   burayı değiştirebilirsin. �?imdilik flag’i bekletelim. */
	            	logInfo("First online not confirmed by HTTP.\r\n");
	            }
	        }

			server_version = nvsesp_parseVersionJson((char*)espWifi.rxBuffer);
			//nvsesp_parseVersionJson((char*)espWifi.rxBuffer);
			if (server_version != g_nv_cfg.version) {

				logInfo("Version mismatch! Local=%lu, Server=%lu\r\n",
			                 (unsigned long)g_nv_cfg.version,
			                 (unsigned long)server_version);

				nvsesp_sendAtCommand(
						"AT+CIPSTART=0,\"TCP\",\"burakozdemir1.pythonanywhere.com\",80",
						2000);
				ESP8266_GetFirmwareMetadata(&totalBlocks);
				if (ESP8266_RequestFirmware(&totalBlocks)
						== ESP_STATUS_METADATA_FAIL)
					NVIC_SystemReset();
			}
		}
		else {
			logInfo("BL DEBUG: Server Connection Error.\n");
			while (1);
		}
	    nv_cfg_ctx_t ctx;
	    if (nv_cfg_init(&ctx) == NV_CFG_OK) {
	        if (nv_cfg_set_version(&ctx, server_version) == NV_CFG_OK) {
	            if (nv_cfg_commit(&ctx) == NV_CFG_OK) {
	                /* RAM globali güncelle */
	                nv_cfg_load_globals();
	                logInfo("Version updated in flash: %lu\r\n",
	                             (unsigned long)g_nv_cfg.version);
	            } else {
	            	logInfo("Version commit FAILED!\r\n");
	            }
	        } else {
	        	logInfo("Version set FAILED!\r\n");
	        }
	    } else {
	    	logInfo("nv_cfg_init FAILED!\r\n");
	    }
	    logInfo("BL DEBUG: Jumping App Code.\n");
		bootloader_jump_to_user_application();

	}

//	if(g_nv_cfg.first_online_flag)
//	{
//		printMessage("BL DEBUG: Jumping App Code.\n");
//		bootloader_jump_to_user_application();
//	}

	else {
		logInfo("Wifi Fail\n");

	    /* Eğer cihaz daha önce hiç internete çıkmadıysa → AP moduna gir (ilk kurulum zorunlu) */
	    if (!g_nv_cfg.first_online_flag) {
	        nvsesp_sendAtCommand("AT+CIPMUX=1", 2500);
	        nvsesp_sendAtCommand("AT+CIPDINFO=1", 2000);
	        nvsesp_sendAtCommand("AT+CIPSERVER=1,80", 2000);
	        nvsesp_apModeGetDataInit();

	        /* Not: İlk kurulumda internete bağlanana kadar APP’e zıplamıyoruz. */
	    } else {
	        /* Cihaz daha önce internete çıkmış → offline devam etmeye izin ver */
	    	logInfo("Offline mode allowed (first_online_flag=1). Jumping app.\n");
	        bootloader_jump_to_user_application();
	    }
	}






  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CRC Initialization Function
  * @param None
  * @retval None
  */
static void MX_CRC_Init(void)
{

  /* USER CODE BEGIN CRC_Init 0 */

  /* USER CODE END CRC_Init 0 */

  /* USER CODE BEGIN CRC_Init 1 */

  /* USER CODE END CRC_Init 1 */
  hcrc.Instance = CRC;
  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CRC_Init 2 */

  /* USER CODE END CRC_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
