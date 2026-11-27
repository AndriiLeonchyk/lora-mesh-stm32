/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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

#include "sx1276.h"
#include "sx1276_port.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Radio link test. Flash one board with 1 (sends "ping N" every 2 s)
 * and the other with 0 (listens and logs every packet). */
#define RADIO_ROLE_TX      1
#define PING_PERIOD_MS     2000u
#define TX_TIMEOUT_MS      1000u   /* SF7, 20 bytes takes about 60 ms */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
static sx1276_t radio;
static int radio_status = SX1276_ERR_ARGS;
static uint8_t  tx_busy;           /* waiting for TxDone */
static uint32_t tx_started_ms;
#if RADIO_ROLE_TX
static uint32_t last_ping_ms;
static uint32_t ping_counter;
#endif

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* Sends a text line to USART1 (PA9, 115200 8N1) for a USB-UART adapter. */
static void log_line(const char *text)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)strlen(text), 100);
}

/* PC13 LED is active low. */
static void led(int on)
{
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void led_flashes(unsigned count, uint32_t on_ms, uint32_t off_ms)
{
  for (unsigned i = 0; i < count; i++) {
    led(1);
    HAL_Delay(on_ms);
    led(0);
    HAL_Delay(off_ms);
  }
}

/* Radio did not start. The number of flashes tells what was read from
 * RegVersion, so wiring problems can be found without a UART adapter:
 *   2 flashes: 0x00  (no power, MISO stuck low, NSS/SCK not reaching the chip)
 *   3 flashes: 0xFF  (MISO not connected / floating, chip held in reset)
 *   4 flashes: version read but other error (unstable contact, SPI error) */
static void show_radio_error(void)
{
  unsigned flashes = 4;
  if (radio_status == SX1276_ERR_VERSION && radio.version == 0x00) {
    flashes = 2;
  } else if (radio_status == SX1276_ERR_VERSION && radio.version == 0xFF) {
    flashes = 3;
  }
  led_flashes(flashes, 200, 300);
  HAL_Delay(1500);
}

/* Reads one radio event (after DIO0) and logs it. */
static void handle_radio_event(void)
{
  /* static: the stack is only 1 KB (CubeMX default), keep big buffers off it. */
  static char line[128];
  static uint8_t data[255];
  sx1276_event_t ev = SX1276_EVENT_NONE;

  if (sx1276_handle_dio0(&radio, &ev) != SX1276_OK) {
    log_line("DIO0: SPI error\r\n");
    return;
  }

  switch (ev) {
  case SX1276_EVENT_TX_DONE:
    tx_busy = 0;
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    snprintf(line, sizeof(line), "TX done in %lu ms\r\n",
             (unsigned long)(HAL_GetTick() - tx_started_ms));
    log_line(line);
    break;

  case SX1276_EVENT_RX_DONE: {
    sx1276_rx_info_t info;
    if (sx1276_read_packet(&radio, data, sizeof(data), &info) == SX1276_OK) {
      HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
      snprintf(line, sizeof(line), "RX %u B, RSSI %d dBm, SNR %d dB: %.*s\r\n",
               (unsigned)info.len, (int)info.rssi_dbm, (int)info.snr_db,
               (int)info.len, (const char *)data);
      log_line(line);
    }
    break;
  }

  case SX1276_EVENT_RX_CRC_ERROR:
    log_line("RX: CRC error, packet dropped\r\n");
    break;

  default:
    break;
  }
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
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  /* Reset the SX1276, check RegVersion == 0x12 and configure LoRa
   * (868 MHz, SF7, 10 dBm). */
  sx1276_config_t radio_cfg = sx1276_default_config();
  radio_status = sx1276_init(&radio, sx1276_port_hal(), &radio_cfg);

  char msg[64];
  snprintf(msg, sizeof(msg), "SX1276 init: status %d, RegVersion 0x%02X\r\n",
           radio_status, (unsigned)radio.version);
  log_line(msg);

  if (radio_status == SX1276_OK) {
    led_flashes(3, 80, 80);        /* radio found */
#if RADIO_ROLE_TX
    log_line("Role: transmitter\r\n");
#else
    log_line("Role: receiver\r\n");
    if (sx1276_start_rx(&radio) != SX1276_OK) {
      log_line("start RX failed\r\n");
    }
#endif
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (radio_status != SX1276_OK) {
      show_radio_error();
      continue;
    }

    /* Events from the radio: the interrupt only set a flag. */
    if (sx1276_port_dio0_pending()) {
      handle_radio_event();
    }

#if RADIO_ROLE_TX
    uint32_t now = HAL_GetTick();
    if (tx_busy && now - tx_started_ms > TX_TIMEOUT_MS) {
      tx_busy = 0;                 /* TxDone was missed: do not get stuck */
      log_line("TX timeout\r\n");
    }
    if (!tx_busy && now - last_ping_ms >= PING_PERIOD_MS) {
      char ping[24];
      int n = snprintf(ping, sizeof(ping), "ping %lu", (unsigned long)ping_counter++);
      if (sx1276_send(&radio, (const uint8_t *)ping, (uint8_t)n) == SX1276_OK) {
        tx_busy = 1;
        tx_started_ms = now;
      } else {
        log_line("send failed\r\n");
      }
      last_ping_ms = now;
    }
#endif
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

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
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(RADIO_NSS_GPIO_Port, RADIO_NSS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(RADIO_RESET_GPIO_Port, RADIO_RESET_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : RADIO_NSS_Pin */
  GPIO_InitStruct.Pin = RADIO_NSS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(RADIO_NSS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : RADIO_RESET_Pin */
  GPIO_InitStruct.Pin = RADIO_RESET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(RADIO_RESET_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : RADIO_DIO0_Pin */
  GPIO_InitStruct.Pin = RADIO_DIO0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(RADIO_DIO0_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(RADIO_DIO0_EXTI_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(RADIO_DIO0_EXTI_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* Called by the HAL from EXTI1_IRQHandler when DIO0 (PB1) goes high.
 * Runs in interrupt context: only record the event, no SPI here. */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == RADIO_DIO0_Pin) {
    sx1276_port_dio0_isr();
  }
}

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
