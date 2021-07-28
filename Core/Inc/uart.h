/**
 * @file uart.h
 * @author Timothy Nguyen
 * @brief Interface for UART peripheral using STM32L4 LL library.
 * @version 0.1
 * @date 2021-07-12
 *
 * Notes:
 * - A UART peripheral must be initialized prior to using this module.
 * - Do not enable UART interrupts within CubeMX.
 */

#ifndef _UART_H_
#define _UART_H_

#include "common.h"
#include "stm32l4xx_ll_usart.h"

/* Configuration parameters */
#define UART_TX_BUF_SIZE 1024 // Maximum number of bytes in UART's transmit circular buffer.

/* Configuration structure */
typedef struct
{
    USART_TypeDef *uart_reg_base; // Address of UARTn peripheral's base register.
    IRQn_Type irq_num;            // Interrupt request number (IRQn) of UARTn peripheral.
} uart_config_t;

/**
 * @brief Initialize UART module with configuration structure.
 * 
 * @param uart_cfg UART configuration structure containing hardware definitions.
 * 
 * @return MOD_OK if initialization was successful, else a "MOD_ERR" value.
 */
mod_err_t uart_init(uart_config_t *uart_cfg);

/**
 * @brief Start reception and transmision of characters over UART by enabling interrupts.
 * 
 * @return MOD_OK for success, else a "MOD_ERR" value.
 */
mod_err_t uart_start(void);

/** 
 * @brief Put a character for transmission in transmit buffer (non-blocking).
 *
 * @param c Character to transmit.
 * 
 * @return MOD_OK for success, else a "MOD_ERR" value.
 * 
 * @note Characters may be placed in transmit buffer before interrupts are enabled to start transmission.
 */
mod_err_t uart_putc(char c);

#endif
