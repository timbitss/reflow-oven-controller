/**
 * @file uart.c
 * @author Timothy Nguyen
 * @brief Interface for UART peripheral using STM32L4 LL and HAL library.
 * @version 0.1
 * @date 2021-07-12
 */

#include <stdio.h>

#include "uart.h"
#include "printf.h"
#include "common.h"
#include "cmd.h"
#include "stm32l4xx_ll_usart.h"
#include "stm32l4xx_hal.h"
#include "string.h"
#include "log.h"
#include "console.h"

////////////////////////////////////////////////////////////////////////////////
// Common macros
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief UART peripheral structure.
 */
typedef struct
{
    /* Configuration parameters */
    USART_TypeDef *uart_reg_base; // Pointer to UART's base register address.
    IRQn_Type irq_num;

    /* Data */
    uint16_t tx_buf_get_idx;       // Transmit buffer get index.
    uint16_t tx_buf_put_idx;       // Transmit buffer put index.
    char tx_buf[UART_TX_BUF_SIZE]; // Transmit circular buffer.
} UART_t;

/**
 * @brief List of UART performance measurements.
 */
typedef enum
{
    CNT_RX_UART_ORE,    // Overrun error count.
    CNT_RX_UART_NE,     // Noise detection error count.
    CNT_RX_UART_FE,     // Frame error count.
    CNT_RX_UART_PE,     // Parity error count.
    CNT_TX_BUF_OVERRUN, // Tx buffer overrun count.
    CNT_RX_BUF_OVERRUN, // Rx buffer overrun count.

    NUM_U16_PMS // Number of performance measurements
} UART_pms_t;

////////////////////////////////////////////////////////////////////////////////
// Private (static) variables
////////////////////////////////////////////////////////////////////////////////

/* UART_t Instance */
static UART_t uart;

/* Performance measurement counters */
static uint16_t uart_pms[NUM_U16_PMS];

/* Performance measurement names */
static const char *pm_names[] = {
    "ORE",
    "NE",
    "FE",
    "PE",
    "TX BUF ORE",
    "RX BUF ORE"};

/* Log module client info */
static cmd_client_info uart_client_info =
    {
        .client_name = "uart",
        .num_cmds = 0,
        .cmds = NULL,
        .num_u16_pms = NUM_U16_PMS,
        .u16_pms = uart_pms,
        .u16_pm_names = pm_names};

/* Unique tag for logging module */
static const char *TAG = "UART";

////////////////////////////////////////////////////////////////////////////////
// Private (static) function prototypes
////////////////////////////////////////////////////////////////////////////////

/* UART interrupt service routine. */
static void UART_ISR(void);

/* Read byte from receive data register. */
static inline void read_rdr(void);

/* Write byte to transmit data register. */
static inline void write_tdr(void);

////////////////////////////////////////////////////////////////////////////////
// Public (global) functions
////////////////////////////////////////////////////////////////////////////////

mod_err_t uart_init(uart_config_t *uart_cfg)
{
    if (uart_cfg->uart_reg_base == NULL)
    {
        return MOD_ERR_ARG;
    }
    else if (!LL_USART_IsEnabled(uart_cfg->uart_reg_base))
    {
        return MOD_ERR_PERIPH;
    }
    else
    {
        switch (uart_cfg->irq_num)
        {
        case USART1_IRQn:
        case USART2_IRQn:
        case USART3_IRQn:
        case UART4_IRQn:
        case UART5_IRQn:
            memset(&uart, 0, sizeof(uart));
            uart.irq_num = uart_cfg->irq_num;
            uart.uart_reg_base = uart_cfg->uart_reg_base;
            mod_err_t err = cmd_register(&uart_client_info);
            LOGI(TAG, "Initialized UART");
            return err;
        default:
            return MOD_ERR_ARG;
        }
    }
}

mod_err_t uart_start(void)
{
    if (uart.uart_reg_base == NULL)
    {
        LOGE(TAG, "UART not initialized");
        return MOD_ERR_NOT_INIT;
    }

    LL_USART_EnableIT_TXE(uart.uart_reg_base);  // Generate interrupt whenever TXE flag is set.
    LL_USART_EnableIT_RXNE(uart.uart_reg_base); // Generate interrupt whenever RXNE flag is set.

    /* Numerical interrupt priority must be set greater than or
     * equal to configMAX_SYSCALL_INTERRUPT_PRIORITY
     * in order for ISR to use FreeRTOS API.
     * See https://www.freertos.org/RTOS-Cortex-M3-M4.html */
    __NVIC_SetPriority(uart.irq_num, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));

    __NVIC_EnableIRQ(uart.irq_num);

    return MOD_OK;
}

mod_err_t uart_putc(char c)
{

    uint16_t next_put_idx = (uart.tx_buf_put_idx + 1) % UART_TX_BUF_SIZE;

    /* Tx circular buffer is full. */
    if (next_put_idx == uart.tx_buf_get_idx)
    {
        INC_SAT_U16(uart_pms[CNT_TX_BUF_OVERRUN]);
        return MOD_ERR_BUF_OVERRUN;
    }

    /* Place char in buffer */
    uart.tx_buf[uart.tx_buf_put_idx] = c;
    uart.tx_buf_put_idx = next_put_idx;

    // Ensure TXE interrupt is enabled.
    if (uart.uart_reg_base != NULL && !LL_USART_IsEnabledIT_TXE(uart.uart_reg_base))
    {
        __disable_irq();
        LL_USART_EnableIT_TXE(uart.uart_reg_base);
        __enable_irq();
    }

    return MOD_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Interrupt handlers
////////////////////////////////////////////////////////////////////////////////

void USART1_IRQHandler(void)
{
    UART_ISR();
}

void USART2_IRQHandler(void)
{
    UART_ISR();
}

void USART3_IRQHandler(void)
{
    UART_ISR();
}

void UART4_IRQHandler(void)
{
    UART_ISR();
}

void UART5_IRQHandler(void)
{
    UART_ISR();
}

////////////////////////////////////////////////////////////////////////////////
// Private (static) function definitions
////////////////////////////////////////////////////////////////////////////////

static void UART_ISR(void)
{
    /* Read interrupt status register. */
    uint32_t status_reg = uart.uart_reg_base->ISR;

    /* Service interrupt flags. */
    if (status_reg & USART_ISR_RXNE_Msk)
    {
        read_rdr();
    }
    if (status_reg & USART_ISR_TXE_Msk)
    {
        write_tdr();
    }

    /* Check error flags. */
    if (status_reg & (USART_ISR_ORE | USART_ISR_NE | USART_ISR_FE | USART_ISR_PE))
    {
        if (status_reg & LL_USART_ISR_ORE)
        { // An overrun error occurs if a character is received and RXNE has not been reset.
            // The RDR register content is not lost but the shift register is overwritten by incoming data.
            INC_SAT_U16(uart_pms[CNT_RX_UART_ORE]);
            LL_USART_ClearFlag_ORE(uart.uart_reg_base);
        }
        if (status_reg & LL_USART_ISR_NE)
        {
            INC_SAT_U16(uart_pms[CNT_RX_UART_NE]);
            LL_USART_ClearFlag_NE(uart.uart_reg_base);
        }
        if (status_reg & LL_USART_ISR_FE)
        {
            INC_SAT_U16(uart_pms[CNT_RX_UART_FE]);
            LL_USART_ClearFlag_FE(uart.uart_reg_base);
        }
        if (status_reg & LL_USART_ISR_PE)
        {
            INC_SAT_U16(uart_pms[CNT_RX_UART_PE]);
            LL_USART_ClearFlag_PE(uart.uart_reg_base);
        }
    }
}

/**
 * @brief Read character from receive data register (RDR) and send to console message queue.
 */
static inline void read_rdr(void)
{
    char rx_char = uart.uart_reg_base->RDR & 0xFFU; // Clears RXNE flag.
    mod_err_t err = console_post(rx_char);
    if (err == MOD_ERR_TIMEOUT)
    {
        INC_SAT_U16(uart_pms[CNT_RX_BUF_OVERRUN]);
    }
}

/**
 * @brief Write character from transmit buffer to transmit data register (TDR).
 */
static inline void write_tdr(void)
{
    if (uart.tx_buf_get_idx == uart.tx_buf_put_idx)
    {
        /* Nothing to transmit, disable TXE flag from generating an interrupt. */
        LL_USART_DisableIT_TXE(uart.uart_reg_base);
    }
    else
    {
        uart.uart_reg_base->TDR = uart.tx_buf[uart.tx_buf_get_idx]; // Clears TXE flag.
        uart.tx_buf_get_idx = (uart.tx_buf_get_idx + 1) % UART_TX_BUF_SIZE;
    }
}
