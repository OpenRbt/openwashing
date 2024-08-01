/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "relays.h"
#include "gm009605.h"
#include "fonts.h"
#include "buttons.h"
#include "cmsis_os.h"
#include "app.h"
#include <string.h>
#include "menu.h"
#include "bitmap.h"
#include "shared.h"
#include "esq500modbus.h"
#include <stdlib.h>

I2C_HandleTypeDef hi2c1;
int direction = 1;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

menu main_menu;
menu_supervisor main_menu_supervisor;

uint8_t usb_buf_cursor;
char usb_buf[MAX_CMD_BUF + 1];

char motor_tx_buf[16+1];
char motor_buf[16+1];
uint8_t motor_cur;
uint8_t cmd_ready = 0;

char motor_cmd[16];
int needToSend = 0;
// Global vars here for an unknown reason.
// It feels like when they are here the performance increases.
gpio_entity keys;
buttons btns;
uint8_t btns_clicked[BUTTONS_COUNT];

/* Definitions for command_reader */
osThreadId_t command_readerHandle;
const osThreadAttr_t command_reader_attributes = {
  .name = "command_reader",
  .priority = (osPriority_t) osPriorityLow,
  .stack_size = 256 * 4
};
/* Definitions for user_interface */
osThreadId_t user_interfaceHandle;
const osThreadAttr_t user_interface_attributes = {
  .name = "user_interface",
  .priority = (osPriority_t) osPriorityLow,
  .stack_size = 128 * 4
};
/* Definitions for keys_switcher */
osThreadId_t keys_switcherHandle;
const osThreadAttr_t keys_switcher_attributes = {
  .name = "keys_switcher",
  .priority = (osPriority_t) osPriorityLow,
  .stack_size = 128 * 4
};
/* Definitions for rs485_control */
osThreadId_t rs485_controlHandle;
const osThreadAttr_t rs485_control_attributes = {
  .name = "rs485_control",
  .priority = (osPriority_t) osPriorityLow,
  .stack_size = 128 * 4
};
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);

void Start_command_reader(void *argument);
void Start_user_interface(void *argument);
void Start_keys_switcher(void *argument);
void Start_rs485_controller(void *argument);


void send_usb(const char * cmd, int size) {
	send_bytes(1, cmd, size);
}
void send_rs485(const char * cmd, int size) {
	send_bytes(2, cmd, size);
}

void void_os_delay(uint32_t ticks) {
	osDelay(ticks);
}

int main(void) {
	HAL_Init();
	app_init();
	set_app_send_data(send_usb);
	set_rs485_write(send_rs485);
	set_turn_on_func(turn_on);
	set_turn_off_func(turn_off);
	set_sleep_func(void_os_delay);
	SystemClock_Config();
	MX_GPIO_Init();
	MX_I2C1_Init();
	MX_USART1_UART_Init();
	MX_USART2_UART_Init();
	osKernelInitialize();
	init_gpio_entity(&keys);
	init_buttons(&btns);
	init_usarts();
	SSD1306_Init();
	set_incoming_byte_handler(1, receive_cmd);
	set_incoming_byte_handler(2, receive_motor1);
	set_state(ST_WAITING_FOR_CONNECTION);
	menu_init(&main_menu, &main_menu_supervisor);
	menu_supervisor_init(&main_menu_supervisor, &main_menu);
	usb_buf[0] = 0;
	usb_buf_cursor = 0;
	motor_cur = 0;
	/*
	motor_tx_buf[0] = 49;
	motor_tx_buf[1] = 50;
	motor_tx_buf[2] = 10;
	*/

	set_tick_func(HAL_GetTick);


	motor_buf[0] = 0;
	motor_tx_buf[0] = 0x01;
	motor_tx_buf[1] = 0x06;
	motor_tx_buf[2] = 0x1e;
	motor_tx_buf[3] = 0x02;
	motor_tx_buf[4] = 0x00;
	motor_tx_buf[5] = 0x01;
	motor_tx_buf[6] = 0x23;
	motor_tx_buf[7] = 0xe2;

	//needToSend = 0;

	command_readerHandle = osThreadNew(Start_command_reader, NULL, &command_reader_attributes);
	user_interfaceHandle = osThreadNew(Start_user_interface, NULL, &user_interface_attributes);

	/* creation of keys_switcher */
	keys_switcherHandle = osThreadNew(Start_keys_switcher, NULL, &keys_switcher_attributes);

	/* creation of rs485_control */
	rs485_controlHandle = osThreadNew(Start_rs485_controller, NULL, &rs485_control_attributes);

	/* Start scheduler */
	osKernelStart();

	/* We should never get here as control is now taken by the scheduler */

	while (1) {
		osDelay(1000);
	}
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1|RCC_PERIPHCLK_I2C1;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void) {
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x0000020B;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK) {
    Error_Handler();
  }
}

static void MX_USART1_UART_Init(void) {
	huart1.Instance = USART1;
	if (get_connection_mode() == CONNECTION_RS485) {
		huart1.Init.BaudRate = 9600;
	} else {
		huart1.Init.BaudRate = 38400;
	}
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_RS485Ex_Init(&huart1, UART_DE_POLARITY_HIGH, 0, 0) != HAL_OK) {
		Error_Handler();
	}

	USART1->CR1 |= USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
	USART1->CR2 = 0;
	USART1->CR3 = 0;
	NVIC_EnableIRQ(USART1_IRQn);
}

static void MX_USART2_UART_Init(void) {
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_RS485Ex_Init(&huart2, UART_DE_POLARITY_HIGH, 0, 0) != HAL_OK) {
    Error_Handler();
  }
  USART2->CR1 &= ~  (USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_TXEIE);
  //USART2->CR1 |= USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
  //| USART_CR1_RXNEIE;
  USART2->CR2 = 0;
  USART2->CR3 = 0;
  NVIC_EnableIRQ(USART2_IRQn);
}

static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, OUT_11_Pin|OUT_10_Pin|OUT_9_Pin|OUT_8_Pin
                          |OUT_7_Pin|OUT_1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, OUT_6_Pin|OUT_5_Pin|OUT_4_Pin|OUT_3_Pin
                          |OUT_2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : BUTTON_1_Pin BUTTON_2_Pin BUTTON_3_Pin */
  GPIO_InitStruct.Pin = BUTTON_1_Pin|BUTTON_2_Pin|BUTTON_3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : OUT_11_Pin OUT_10_Pin OUT_9_Pin OUT_8_Pin
                           OUT_7_Pin OUT_1_Pin */
  GPIO_InitStruct.Pin = OUT_11_Pin|OUT_10_Pin|OUT_9_Pin|OUT_8_Pin
                          |OUT_7_Pin|OUT_1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : OUT_6_Pin OUT_5_Pin OUT_4_Pin OUT_3_Pin
                           OUT_2_Pin */
  GPIO_InitStruct.Pin = OUT_6_Pin|OUT_5_Pin|OUT_4_Pin|OUT_3_Pin
                          |OUT_2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

char received_cmd[MAX_CMD_BUF+1] = {0,};
void Start_command_reader(void *argument) {
  for(;;) {
	  if (cmd_ready) {
		  cmd_ready = 0;
		  app_process_cmd(received_cmd);
		  received_cmd[0] = 0;
	  }
	  osDelay(1);
  }
}

void display_logo(int key) {
	SSD1306_Clear();
	SSD1306_DrawBitmap(0, 0, bg, 128, 64, 1);
	if (key == 1) SSD1306_DrawBitmap(72, 8, rb1, 48, 54, 1);
	if (key == 2) SSD1306_DrawBitmap(72, 8, rb2, 48, 54, 1);
	if (key == 3) SSD1306_DrawBitmap(72, 8, rb3, 48, 54, 1);
	SSD1306_UpdateScreen();
}

void display_connection(int is_connected) {
	SSD1306_Clear();
	if(!is_connected) {
		SSD1306_GotoXY(25, 3);
		SSD1306_Puts("WAITING", &Font_11x18, 1);
		SSD1306_GotoXY(47, 23);
		SSD1306_Puts("FOR", &Font_11x18, 1);
		SSD1306_GotoXY(9, 45);
		SSD1306_Puts("CONNECTION", &Font_11x18, 1);
	} else {
		SSD1306_GotoXY(9, 45);
		uint8_t x = rand() % 28;
		uint8_t y = rand() % 45;
		SSD1306_GotoXY(x, y);
		SSD1306_Puts("CONNECTED", &Font_11x18, 1);
	}
	SSD1306_UpdateScreen();
}

void display_post_num() {
	SSD1306_Clear();
	uint8_t x = rand() % 95;
	uint8_t y = rand() % 37;

	SSD1306_GotoXY(x, y);
	SSD1306_Puts(get_post_num_str(), &Font_16x26, 1);
	SSD1306_UpdateScreen();
}

void display_motor_status() {
	SSD1306_Clear();
	uint8_t motor_com_status = get_motor_communication_status();
	uint8_t y = rand() % 45;
	if (motor_com_status == MOTOR_NOT_ANSWERING){
		uint8_t x = rand() % 39;
		SSD1306_GotoXY(x, y);
		SSD1306_Puts("NO MOTOR", &Font_11x18, 1);
	} else if (motor_com_status == MOTOR_WRONG_ANSWERS) {
		int8_t x = rand() % 28;
		SSD1306_GotoXY(x, y);
		SSD1306_Puts("MOTOR ERR", &Font_11x18, 1);
	} else {
		uint8_t x = rand() % 39;
		SSD1306_GotoXY(x, y);
		SSD1306_Puts("MOTOR OK", &Font_11x18, 1);
	}
	SSD1306_UpdateScreen();
}

void start_animation();

void Start_user_interface(void *argument) {
	start_animation();
	needToSend = 0;
	uint16_t loop = 1000;
	uint8_t frame = 0;
	uint8_t mode = MODE_SCREENSAVER;
	for(;;) {
		if (mode == MODE_SCREENSAVER) {
			loop++;
			// Will be state machine later


			// cleaning states of the buttons:
			is_dblclicked(0);
			if(is_dblclicked(2)) {
				const char * cmd = "RUN T60|1/900/900|2/850/850|3/800/800|4/750/750|5/725/725|6/700/700|7/650/650|8/630/630|9/600/600|10/550/550|11/500/500|;";
				app_process_cmd(cmd);
			}
			is_clicked(0);
			//is_clicked(2);

			if(is_dblclicked(1)) {
				mode = MODE_MENU;
				loop = 0;
				continue;
			}

			if (loop>1000) {
				loop = 0;
				if (frame == 0) {
					display_connection(get_state() != ST_WAITING_FOR_CONNECTION);
				}
				if (frame == 1) {
					display_post_num();
				}
				if (frame == 2) {
					display_motor_status();
				}
				if (frame == 3) {
					SSD1306_Clear();
					SSD1306_UpdateScreen();
				}
				if (frame == 3) {
					SSD1306_Clear();
					SSD1306_UpdateScreen();
				}

				frame++;
				if(frame >= 3) frame = 0;
			}
		}
		if (mode == MODE_MENU) {
			if (loop == 0) {
				display_menu_supervisor(&main_menu_supervisor);
			}
			if(main_menu_supervisor.exit) {
				main_menu_reset();
				mode = MODE_SCREENSAVER;
				loop+=1000;
			}
			loop++;
			if (loop > 10000) {
				main_menu_reset();
				mode = MODE_SCREENSAVER;
				loop+=1000;
			}
			if (is_clicked(0)) {
				loop = 0;
				menu_supervisor_up(&main_menu_supervisor);
			}
			if (is_clicked(1)) {
				loop = 0;
				menu_supervisor_center(&main_menu_supervisor);
			}
			if (is_clicked(2)) {
				loop = 0;
				menu_supervisor_down(&main_menu_supervisor);
			}

		}
		osDelay(1);
	}
}

void main_menu_reset() {
	is_clicked(0);
	is_clicked(1);
	is_clicked(2);
	is_dblclicked(0);
	is_dblclicked(1);
	is_dblclicked(2);
	main_menu_supervisor.exit = 0; // Let's reset the flag
	main_menu_supervisor.cursor = 0;
	main_menu_supervisor.cur_item = &main_menu;
	main_menu_supervisor.start_item = 0;
}

void start_animation() {
	SSD1306_Clear();
	SSD1306_GotoXY(0,0);
	SSD1306_Mirror(1, 1);
	SSD1306_ON();
	display_logo(1);
	osDelay(500);
	display_logo(2);
	osDelay(500);
	display_logo(3);
	osDelay(500);
	display_logo(2);
	osDelay(500);
	display_logo(1);
	osDelay(2000);
}

void Start_keys_switcher(void *argument) {
  int frame = 0;
  for(int i=0;i<11;i++) turn_off(i);
  for(;;) {
	frame++;
	if(frame>5) {
		buttons_update();
		frame = 0;
	}
	app_update_relays();
    osDelay(1);
  }
}

void Start_rs485_controller(void *argument) {
	// Let's not touch the motors for 3 seconds
	osDelay(3000);
	for(;;) {
		motor_loop();
		osDelay(1);
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
  if (htim->Instance == TIM6) {
    HAL_IncTick();
  }
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

void turn_on(uint8_t key) {
	HAL_GPIO_WritePin(keys.relays[key].port, keys.relays[key].pin , GPIO_PIN_SET);
}

void turn_off(uint8_t key) {
	HAL_GPIO_WritePin(keys.relays[key].port, keys.relays[key].pin , GPIO_PIN_RESET);
}

void buttons_update() {
	for(uint8_t i=0;i<BUTTONS_COUNT;i++) {
		update_button(&btns.btn[i]);
	}
}

uint8_t is_acceptable(char k) {
	if (k>='0' && k <='9') return 1;
	if (k=='+' || k=='-' || k=='|' || k=='-' || k=='/' || k==';' || k==':' || k=='?' || k==')' ||k=='.'|| k==',' || k==' '|| k=='!') return 1;
	if (k>='a' && k<='z') return 1;
	if (k>='A' && k<='Z') return 1;
	return 0;
}

void receive_cmd(uint8_t sym) {
	if (sym == '\n') usb_buf_cursor = 0;
	if(!is_acceptable(sym)) return;
	usb_buf[usb_buf_cursor+1] = 0;
	usb_buf[usb_buf_cursor] = (char)sym;
	usb_buf_cursor++;
	if(usb_buf_cursor>=MAX_CMD_BUF) {
		usb_buf_cursor = MAX_CMD_BUF - 1;
	}
	if(sym == ';') {
		cp(usb_buf, received_cmd, 0);
		usb_buf_cursor = 0;
		cmd_ready = 1;
	}
}

void receive_motor1(uint8_t sym) {
	app_push_motor_byte(sym);
}

uint8_t is_clicked(uint8_t key) {
  if(btns.btn[key].is_clicked) {
	  btns.btn[key].is_clicked = 0;
	  return 1;
  }
  return 0;
}

uint8_t is_dblclicked(uint8_t key) {
	if(btns.btn[key].is_dblclicked) {
		btns.btn[key].is_clicked = 0;
		btns.btn[key].is_dblclicked = 0;
		return 1;
	}
	return 0;
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
