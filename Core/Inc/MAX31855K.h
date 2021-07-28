/**
 * @file MAX31855K.h
 * @author Timothy Nguyen
 * @brief  MAX31855K SparkFun Breakout Board API.
 * @version 0.1
 * @date 2021-07-07
 * 
 * @note API inspired by SparkFun's MAX31855K Arduino library.
 * 
 * MAX31855K Memory Map:
 * D[31:18] Signed 14-bit cold-junction compensated thermocouple temperature (HJ temperature)
 * D17      Reserved: Always reads 0
 * D16      Fault: 1 when any of the SCV, SCG, or OC faults are active, else 0
 * D[15:4]  Signed 12-bit internal temperature
 * D3       Reserved: Always reads 0
 * D2       SCV fault: Reads 1 when thermocouple is shorted to V_CC, else 0
 * D1       SCG fault: Reads 1 when thermocouple is shorted to gnd, else 0
 * D0       OC  fault: Reads 1 when thermocouple is open-circuit, else 0
 */

#ifndef _MAX31855K_H_
#define _MAX31855K_H_

#include "stm32l4xx_hal.h"
#include "stm32l476xx.h"

// MAX31855K thermocouple device error definitions.
typedef enum
{
    MAX_OK,           // Successful temperature reading.
    MAX_SHORT_VCC,    // Thermocouple shorted to VCC.
    MAX_SHORT_GND,    // Thermocouple shorted to GND.
    MAX_OPEN,         // Thermocouple connection is open.
    MAX_ZEROS,        // SPI read only 0s.
    MAX_SPI_DMA_FAIL, // Error during SPI DMA RX transfer.

    MAX_NUM_ERRORS
} MAX31855K_err_t;

/* MAX31855K configuration structure. */
typedef struct
{
    SPI_HandleTypeDef *hspi;   // SPI Handler instance.
    GPIO_TypeDef *max_cs_port; // GPIO port for MAX31855K chip-select.
    uint16_t max_cs_pin;       // GPIO pin number of MAX31855K chip-select.
} MAX31855K_cfg_t;

/**
 * @brief Initialize MAX31885K driver.
 * 
 * @param max_cfg Configuration parameters.
 */
void MAX31855K_Init(MAX31855K_cfg_t const *const max_cfg);

/**
 * @brief Read data from MAX31855K in blocking mode and check for errors.
 * 
 * @return MAX31855K_err_t Error value.
 * 
 * SPI instance must be initialized prior to function call.
 */
MAX31855K_err_t MAX31855K_RxBlocking();

/**
 * @brief Read data from MAX31855K in non-blocking mode through DMA controller.
 */
void MAX31855K_RxDMA();

/**
 * @brief Format data received and check for errors after DMA transfer.
 * 
 * @param max Pointer to MAX321885K_t structure containing configuration parameters and data.
 * 
 * Function should be called from within a SPI_RX_Cplt callback function.
 */
void MAX31885K_RxDMA_Complete();

/**
 * @brief Parse HJ temperature from raw data.
 * 
 * @pre Check that max's error value equals MAX_OK.
 *
 * @param max Pointer to MAX321885K_t structure containing configuration parameters and data.
 *
 * @return float Hot junction temperature.
 */
float MAX31855K_Get_HJ();

/**
 * @brief Parse CJ temperature from raw data.
 * 
 * @pre Check that max's error value equals MAX_OK.
 *
 * @param max Pointer to MAX321885K_t structure containing configuration parameters and data.
 *
 * @return float Cold junction temperature.
 */
float MAX31855K_Get_CJ();

/**
 * @brief Get current error value as a character string.
 *
 * @return Error value formatted as character string.
 */
const char *MAX31855K_Err_Str();

#endif
