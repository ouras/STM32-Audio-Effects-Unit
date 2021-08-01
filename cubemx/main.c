/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "effects.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* Data convention definitions */
#define X_AXIS (UINT16_MAX / 2)     // x-axis is the median representable signal

#ifndef FIXED_POINT_Q
#define FIXED_POINT_Q 8   // N in QN for fixed-point numbers
#endif

/* DMA Option definitions */
#define SOFTWARE_WAIT_FOR_ADC_DMA	// Define to stop software when ADC/DMA has not caught up (will re-use old data otherwise)

//#define ADC_DMA_WAIT_FOR_SOFTWARE	// Define to stop the ADC/DMA when software has not caught up (will overwrite data otherwise)

//#define DAC_DMA_WAIT_FOR_SOFTWARE	// Define to stop the DAC/DMA when software has not caught up (will re-output old data otherwise)

#define SOFTWARE_WAIT_FOR_DAC_DMA	// Define to stop software when DAC/DMA has not caught up (will overwrite data otherwise)

/* Buffer definitions */
#define ADC_BUF_LEN (2 * 200)	// Ping-pong buffer of uint16_t (circular buffer of 2 buffers)

#define DAC_BUF_LEN ADC_BUF_LEN	// Ping-pong buffer of uint16_t with enough space for two batches of outputs

#define NUM_DELAY_SAMPLES MAX_ECHO_DELAY_SAMPLES	// Number of extra delayed samples for echo

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

DAC_HandleTypeDef hdac;
DMA_HandleTypeDef hdma_dac1;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* Effect parameters */
extern overdrive_param overdrive;
extern echo_param echo;
extern compression_param compression;

volatile bool overdrive_enabled = true;
volatile bool echo_enabled = true;
volatile bool compression_enabled = true;

// DMA buffer for ADC samples (note: samples are in order of decreasing age)
uint16_t adc_buf[ADC_BUF_LEN] = {0};

// Pointers to each half of ADC/DMA buffer for ping-pong buffering (note: DMA sees one buffer)
uint16_t* const adc_buf_a = adc_buf;
uint16_t* const adc_buf_b = &adc_buf[ADC_BUF_LEN / 2];

// Extra buffer for carrying over enough past samples for echo
uint16_t delay_samples_buf[NUM_DELAY_SAMPLES] = {0};

// Flags controlled by DMA (callbacks) and software
volatile bool adc_buf_a_full;
volatile bool adc_buf_b_full;
volatile bool dac_buf_a_full;
volatile bool dac_buf_b_full;

// Buffer for filter outputs, big enough to also store edge outputs of each ADC buffer (note: samples are in order of decreasing age)
uint16_t dac_buf[DAC_BUF_LEN] = {0};

// Pointers to each half of the output buffer (note: DMA sees one buffer)
uint16_t* const dac_buf_a = dac_buf;
uint16_t* const dac_buf_b = &dac_buf[DAC_BUF_LEN / 2];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_DAC_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

bool adc_enabled(ADC_HandleTypeDef *handle);
bool dac_ch1_enabled(DAC_HandleTypeDef *handle);
bool dac_ch1_underrun(DAC_HandleTypeDef *handle);
void dac_ch1_dma_reset(DAC_HandleTypeDef *hdac, DMA_HandleTypeDef *hdma, uint32_t *pData, uint32_t Length);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
	adc_buf_a_full = false;
	adc_buf_b_full = false;
	dac_buf_a_full = false;
	dac_buf_b_full = false;
	
	// Set up parameters
  overdrive.level = 161;	// == 130 mV from the ADC
  overdrive.mix = 1 << (FIXED_POINT_Q - 1);	// == 0.5 in QN

	echo.delay_samples = 15;
  echo.pre_delay = 1342;	// To get 30 ms pre-delay at 44.72 kHz, 1342 samples are needed
  echo.density = 1 << (FIXED_POINT_Q - 1);
  echo.attack = 1 << (FIXED_POINT_Q - 1);
  echo.decay = 25;	// 25 is about 0.1*2^8

  compression.threshold = 150;
  compression.ratio = 1 << (FIXED_POINT_Q - 1);
	
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
  MX_ADC1_Init();
  MX_USART2_UART_Init();
  MX_DAC_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
	HAL_ADC_Start_DMA(&hadc1, (uint32_t *) adc_buf, ADC_BUF_LEN);
	HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t *) dac_buf, DAC_BUF_LEN, DAC_ALIGN_12B_R);
	HAL_TIM_OC_Start(&htim2, TIM_CHANNEL_1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
		/******************** ADC/DMA Ping buffer ********************/
		#ifdef SOFTWARE_WAIT_FOR_ADC_DMA
		while (!adc_buf_a_full);
		#endif
		
		#ifdef ADC_DMA_WAIT_FOR_SOFTWARE
		if (!adc_enabled(&hadc1))
		{
			HAL_ADC_Start_DMA(&hadc1, (uint32_t *) adc_buf, ADC_BUF_LEN);
			//HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
		}
		#endif
		
		#ifdef DAC_DMA_WAIT_FOR_SOFTWARE
		if (!dac_ch1_enabled(&hdac))
		{
			HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t *) dac_buf, DAC_BUF_LEN, DAC_ALIGN_12B_R);
			//HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
		}
		#endif
		
		#ifdef SOFTWARE_WAIT_FOR_DAC_DMA
		while (dac_buf_a_full)
		{
			#ifdef DAC_DMA_WAIT_FOR_SOFTWARE
			// With DAC_DMA_WAIT_FOR_SOFTWARE enabled, DAC/DMA may turn off or be underrun
			if (!dac_ch1_enabled(&hdac))
			{
				HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t *) dac_buf, DAC_BUF_LEN, DAC_ALIGN_12B_R);
			}
			if (!dac_ch1_underrun(&hdac))
			{
				// Restart DAC/DMA
				dac_ch1_dma_reset(&hdac, &hdma_dac1, (uint32_t *) dac_buf, DAC_BUF_LEN);
			}
			#endif
		}
		#endif
		
		uint16_t* temp_buffer = malloc((ADC_BUF_LEN / 2) * sizeof(uint16_t));	// Temp scratchpad buffer for applying multiple effects
		
		// Copy previous samples over for echo
		if (NUM_DELAY_SAMPLES < ADC_BUF_LEN / 2)
		{
			// Simply copy last NUM_DELAY_SAMPLES from adc_buf_a
			memcpy(delay_samples_buf, &adc_buf_a[(ADC_BUF_LEN / 2) - NUM_DELAY_SAMPLES], NUM_DELAY_SAMPLES * sizeof(uint16_t));
		}
		else
		{
			// Copy last (NUM_DELAY_SAMPLES - ADC_BUF_LEN / 2) samples from delay_samples_buf, then all of adc_buf_a
			memcpy(delay_samples_buf, &delay_samples_buf[NUM_DELAY_SAMPLES - (NUM_DELAY_SAMPLES - ADC_BUF_LEN / 2)], (NUM_DELAY_SAMPLES - ADC_BUF_LEN / 2) * sizeof(uint16_t));
			memcpy(&delay_samples_buf[NUM_DELAY_SAMPLES - ADC_BUF_LEN / 2], adc_buf_a, ADC_BUF_LEN / 2);
		}
		
		if (echo_enabled)
		{
			// Merge previous samples and current samples into single buffer
			uint16_t* echo_buf = malloc((NUM_DELAY_SAMPLES + (ADC_BUF_LEN / 2)) * sizeof(uint16_t));
			
			memcpy(echo_buf, delay_samples_buf, NUM_DELAY_SAMPLES * sizeof(uint16_t));
			memcpy(&echo_buf[NUM_DELAY_SAMPLES], adc_buf_a, (ADC_BUF_LEN / 2) * sizeof(uint16_t));
			
			buf_echo(echo_buf, temp_buffer, (ADC_BUF_LEN / 2), &echo);
			
			memcpy(adc_buf_a, temp_buffer, (ADC_BUF_LEN / 2) * sizeof(uint16_t));
		}
		else if (overdrive_enabled)
		{
			buf_overdrive(adc_buf_a, temp_buffer, (ADC_BUF_LEN / 2), &overdrive);
			
			memcpy(adc_buf_a, temp_buffer, (ADC_BUF_LEN / 2) * sizeof(uint16_t));
		}
		else if (compression_enabled)
		{
			buf_compression(adc_buf_a, temp_buffer, (ADC_BUF_LEN / 2), &compression);
		}

		memcpy(dac_buf_a, temp_buffer, (ADC_BUF_LEN / 2) * sizeof(uint16_t));
		
		adc_buf_a_full = false;
		dac_buf_a_full = true;
		
		#ifdef DAC_DMA_WAIT_FOR_SOFTWARE
		buf_count++;
		#endif
		
		/******************** ADC/DMA Pong buffer ********************/
		#ifdef SOFTWARE_WAIT_FOR_ADC_DMA
		while (!adc_buf_b_full);
		#endif
		
		#ifdef ADC_DMA_WAIT_FOR_SOFTWARE
		if (!adc_enabled(&hadc1))
		{
			HAL_ADC_Start_DMA(&hadc1, (uint32_t *) adc_buf, ADC_BUF_LEN);
			//HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
		}
		#endif
		
		#ifdef DAC_DMA_WAIT_FOR_SOFTWARE
		if (!dac_ch1_enabled(&hdac))
		{
			HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t *) dac_buf, DAC_BUF_LEN, DAC_ALIGN_12B_R);
			//HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
		}
		#endif
		
		#ifdef SOFTWARE_WAIT_FOR_DAC_DMA
		while (dac_buf_b_full)
		{
			#ifdef DAC_DMA_WAIT_FOR_SOFTWARE
			// With DAC_DMA_WAIT_FOR_SOFTWARE enabled, DAC/DMA may turn off or be underrun
			if (!dac_ch1_enabled(&hdac))
			{
				HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t *) dac_buf, DAC_BUF_LEN, DAC_ALIGN_12B_R);
			}
			if (!dac_ch1_underrun(&hdac))
			{
				dac_ch1_dma_reset(&hdac, &hdma_dac1, (uint32_t *) dac_buf, DAC_BUF_LEN);
			}
			#endif
		}
		#endif
		
		temp_buffer = malloc((ADC_BUF_LEN / 2) * sizeof(uint16_t));	// Temp scratchpad buffer for applying multiple effects
		
		// Copy previous samples over for echo
		if (NUM_DELAY_SAMPLES < ADC_BUF_LEN / 2)
		{
			// Simply copy last NUM_DELAY_SAMPLES from adc_buf_b
			memcpy(delay_samples_buf, &adc_buf_b[(ADC_BUF_LEN / 2) - NUM_DELAY_SAMPLES], NUM_DELAY_SAMPLES * sizeof(uint16_t));
		}
		else
		{
			// Copy last (NUM_DELAY_SAMPLES - ADC_BUF_LEN / 2) samples from delay_samples_buf, then all of adc_buf_b
			memcpy(delay_samples_buf, &delay_samples_buf[NUM_DELAY_SAMPLES - (NUM_DELAY_SAMPLES - ADC_BUF_LEN / 2)], (NUM_DELAY_SAMPLES - ADC_BUF_LEN / 2) * sizeof(uint16_t));
			memcpy(&delay_samples_buf[NUM_DELAY_SAMPLES - ADC_BUF_LEN / 2], adc_buf_b, ADC_BUF_LEN / 2);
		}
		
		if (echo_enabled)
		{
			// Merge previous samples and current samples into single buffer
			uint16_t* echo_buf = malloc((NUM_DELAY_SAMPLES + (ADC_BUF_LEN / 2)) * sizeof(uint16_t));
			
			memcpy(echo_buf, delay_samples_buf, NUM_DELAY_SAMPLES * sizeof(uint16_t));
			memcpy(&echo_buf[NUM_DELAY_SAMPLES], adc_buf_b, (ADC_BUF_LEN / 2) * sizeof(uint16_t));
			
			buf_echo(echo_buf, temp_buffer, (ADC_BUF_LEN / 2), &echo);
			
			memcpy(adc_buf_b, temp_buffer, (ADC_BUF_LEN / 2) * sizeof(uint16_t));
		}
		else if (overdrive_enabled)
		{
			buf_overdrive(adc_buf_b, temp_buffer, (ADC_BUF_LEN / 2), &overdrive);
			
			memcpy(adc_buf_b, temp_buffer, (ADC_BUF_LEN / 2) * sizeof(uint16_t));
		}
		else if (compression_enabled)
		{
			buf_compression(adc_buf_b, temp_buffer, (ADC_BUF_LEN / 2), &compression);
		}

		memcpy(dac_buf_b, temp_buffer, (ADC_BUF_LEN / 2) * sizeof(uint16_t));
		
		adc_buf_b_full = false;
		dac_buf_b_full = true;
		
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
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 88;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
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
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */
  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV8;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */
	// Cannot change sampling time in CubeMX, so doing it here
	sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief DAC Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC_Init(void)
{

  /* USER CODE BEGIN DAC_Init 0 */

  /* USER CODE END DAC_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC_Init 1 */

  /* USER CODE END DAC_Init 1 */
  /** DAC Initialization
  */
  hdac.Instance = DAC;
  if (HAL_DAC_Init(&hdac) != HAL_OK)
  {
    Error_Handler();
  }
  /** DAC channel OUT1 config
  */
  sConfig.DAC_Trigger = DAC_TRIGGER_T2_TRGO;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC_Init 2 */

  /* USER CODE END DAC_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 1968;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_OC_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_TIMING;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_OC_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

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
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

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

}

/* USER CODE BEGIN 4 */

bool adc_enabled(ADC_HandleTypeDef *handle)
{
	return handle->Instance->CR2 & ADC_CR2_ADON;
}

bool dac_ch1_enabled(DAC_HandleTypeDef *handle)
{
	return handle->Instance->CR & DAC_CR_EN1;
}

bool dac_ch1_underrun(DAC_HandleTypeDef *handle)
{
	return __HAL_DAC_GET_FLAG(handle, DAC_FLAG_DMAUDR1);
	//return (handle->Instance->SR & DAC_SR_DMAUDR1) >> DAC_SR_DMAUDR1_Pos;
}

void dac_ch1_dma_reset(DAC_HandleTypeDef *hdac, DMA_HandleTypeDef *hdma, uint32_t *pData, uint32_t Length)
{
	HAL_DAC_Stop_DMA(hdac, DAC_CHANNEL_1);
	__HAL_DAC_CLEAR_FLAG(hdac, DAC_FLAG_DMAUDR1);
	HAL_DAC_Start_DMA(hdac, DAC_CHANNEL_1, pData, Length, DAC_ALIGN_12B_R);
}

// Called when first half of DMA buffer is filled (adc_buf_a is full)
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)
{
  adc_buf_a_full = true;
	
	#ifdef ADC_DMA_WAIT_FOR_SOFTWARE
	if (adc_buf_b_full)
	{
		// If adc_buf_b is still full, software has not finished processing it, so stop the ADC/DMA
		HAL_ADC_Stop_DMA(&hadc1);
		//HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET); // Turn on LED to indicate overrun
	}
	#endif
}

// Called when DMA buffer is completely filled (adc_buf_b is full)
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
  adc_buf_b_full = true;
	
	#ifdef ADC_DMA_WAIT_FOR_SOFTWARE
	if (adc_buf_a_full)
	{
		// If adc_buf_a is still full, software has not finished processing it, so stop the ADC/DMA
		HAL_ADC_Stop_DMA(&hadc1);
		//HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET); // Turn on LED to indicate overrun
	}
	#endif
}

void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef* hdac)
{
	dac_buf_a_full = false;
	
	#ifdef DAC_DMA_WAIT_FOR_SOFTWARE
	if (!dac_buf_b_full)
	{
		// If dac_buf_b is not full, software has not finished filling it, so stop the DAC/DMA
		HAL_DAC_Stop_DMA((DAC_HandleTypeDef *) &hdac, DAC_CHANNEL_1);
		//HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET); // Turn on LED to indicate overrun
	}
	#endif
}

void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac)
{
	dac_buf_b_full = false;
	
	#ifdef DAC_DMA_WAIT_FOR_SOFTWARE
	if (!dac_buf_a_full)
	{
		// If dac_buf_a is not full, software has not finished filling it, so stop the DAC/DMA
		HAL_DAC_Stop_DMA((DAC_HandleTypeDef *) &hdac, DAC_CHANNEL_1);
		//HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET); // Turn on LED to indicate overrun
	}
	#endif
}

#ifdef DAC_DMA_WAIT_FOR_SOFTWARE
void HAL_DAC_DMAUnderrunCallbackCh1(DAC_HandleTypeDef *hdac)
{
	dac_ch1_dma_reset(hdac, &hdma_dac1, (uint32_t *) dac_buf, DAC_BUF_LEN);
}
#endif

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

#ifdef  USE_FULL_ASSERT
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
