
#include "main.h"
#include "stm32f0xx_it.h"
#include "shared.h"


extern TIM_HandleTypeDef htim6;
uart_user_friendly user_uart1;
uart_user_friendly user_uart2;


// let's use this mutex for remaining bytes
volatile xSemaphoreHandle xMutex;

void NMI_Handler(void) {
  while (1) {
  }
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void) {
  while (1) {
  }
}

void TIM6_IRQHandler(void) {
	HAL_TIM_IRQHandler(&htim6);
}

void init_usart(uart_user_friendly *user_usart, USART_TypeDef * actual_port) {
	user_usart->port = actual_port;

	// sending buffer
	user_usart->remaining_bytes = 0;
	user_usart->send_cursor = 0;
	user_usart->send_cycle_buffer[0] = 0;

	user_usart->event_handler = 0;
}

void init_usarts() {
	init_usart(&user_uart1, USART1);
	user_uart1.can_do_both_dirs = 1;
	init_usart(&user_uart2, USART2);
	user_uart2.can_do_both_dirs = 0;
}

uart_user_friendly * get_usart1() {
	return &user_uart1;
}
uart_user_friendly * get_usart2() {
	return &user_uart2;
}

void set_incoming_byte_handler(uint8_t port_num, void (*event_handler)(uint8_t symbol)) {
	if(port_num == 1) {
		user_uart1.event_handler = event_handler;
	} else {
		user_uart2.event_handler = event_handler;
	}
}

uint8_t send_bytes(uint8_t port_num, const char * new_data, uint8_t size) {
	uart_user_friendly * usart;
	if(port_num == 1) {
		usart = &user_uart1;
	} else {
		usart = &user_uart2;
	}
	usart_sending_off(usart->port);
	if (!usart->can_do_both_dirs) {
		set_rs_mode(RS485_DISABLE);
	}
	if (usart->remaining_bytes > 0) {
		return 3; // let's not allow sending something while the previous operation is in progress
	}
	if (size > MAX_SEND_BUF) {
		return 1;
	}
	uint8_t write_cursor = usart->send_cursor + 1;
	for(uint8_t i = 0; i<size; i++) {
		if(write_cursor>=MAX_SEND_BUF) write_cursor = write_cursor - MAX_SEND_BUF;
		if(write_cursor>=MAX_SEND_BUF) {
			return 2;
		}
		usart->send_cycle_buffer[write_cursor] = new_data[i];
		usart->remaining_bytes++;
		write_cursor++;
	}
	usart_sending_on(usart->port);
	return 0;
}

uint8_t send_byte_from_buffer(uart_user_friendly * user_uart) {
	USART_TypeDef * port = user_uart->port;
	if(user_uart->remaining_bytes == 0) {
		usart_sending_off(port);
		if (!user_uart->can_do_both_dirs) {
			set_rs_mode(RS485_RX);
		}
		return 1;
	}
	if ((port->ISR & USART_ISR_TXE)==0) return 2;
	user_uart->send_cursor = user_uart->send_cursor + 1;

	if(user_uart->send_cursor >= MAX_SEND_BUF) user_uart->send_cursor = 0;
	uint8_t to_be_sent = (uint8_t)user_uart->send_cycle_buffer[user_uart->send_cursor];
	user_uart->remaining_bytes = user_uart->remaining_bytes - 1;
	port->TDR = to_be_sent;
	return 0;
}



void process_accepted_byte(uart_user_friendly * user_uart) {
	uint8_t d = user_uart->port->RDR;

	if(user_uart->event_handler) {
		user_uart->event_handler(d);
	}
	return;

}

void usart_sending_on(USART_TypeDef * usart) {
	usart->CR1 |= USART_CR1_TXEIE | USART_CR1_TE;
}

void usart_sending_off(USART_TypeDef * usart) {
	usart->CR1 &= ~(USART_CR1_TXEIE | USART_CR1_TE);
	usart->CR1 |= USART_CR1_RE  | USART_CR1_RXNEIE;
}

void set_rs_mode(uint8_t new_mode) {
	USART_TypeDef * usart = USART2;

	if (new_mode == RS485_DISABLE) {
		usart->CR1 &= ~(USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_TXEIE);
	} else if (new_mode == RS485_TX) {
		usart->CR1 |= USART_CR1_TE | USART_CR1_TXEIE;
		usart->CR1 &= ~(USART_CR1_RE  | USART_CR1_RXNEIE);
	} else {
		usart->CR1 &= ~(USART_CR1_TE | USART_CR1_TXEIE);
		usart->CR1 |= USART_CR1_RE  | USART_CR1_RXNEIE;
	}
}

void default_usart_handler(uart_user_friendly * user_uart) {
	USART_TypeDef * usart = user_uart->port;
	if(usart->ISR & USART_ISR_RXNE) {
		process_accepted_byte(user_uart);
		return;
	}
	if(usart->ISR & USART_ISR_ORE) {
		usart->ICR = (UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_PEF | UART_CLEAR_FEF);
		usart->CR1 &= ~USART_CR1_IDLEIE;
		return;
	}
	if(usart->ISR & USART_ISR_TXE) {
		send_byte_from_buffer(user_uart);
		return;
	}
	if(usart->ISR & USART_ISR_FE) {
		usart->ICR = (UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_PEF | UART_CLEAR_FEF);
		usart->CR1 &= ~USART_CR1_IDLEIE;
	}
}

uint32_t flash_read(){
	uint32_t * res = (uint32_t*)(FLASH_BANK1_END - 2047);
    return res[0];
}

void flash_write(uint32_t data){
    FLASH_EraseInitTypeDef EraseInitStruct;
    EraseInitStruct.PageAddress = FLASH_BANK1_END - 2047;
    EraseInitStruct.TypeErase = TYPEERASE_PAGES;
    EraseInitStruct.NbPages = 1;
    uint32_t PageError = 0;
    if (HAL_FLASH_Unlock() == HAL_OK) {
    	if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) == HAL_OK) {
    		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FLASH_BANK1_END - 2047, data);
    	}
    	HAL_FLASH_Lock();
    }
}

void USART1_IRQHandler(void) {
	default_usart_handler(&user_uart1);
}
void USART2_IRQHandler(void) {
	default_usart_handler(&user_uart2);
}
