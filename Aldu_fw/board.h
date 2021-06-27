/*
 * board.h
 *
 *  Created on: 12 ����. 2015 �.
 *      Author: Kreyl
 */

#pragma once

#include <inttypes.h>

// ==== General ====
#define BOARD_NAME          "Aldu10"
#define APP_NAME            "Arc"

// MCU type as defined in the ST header.
#define STM32F205xx

// Freq of external crystal if any. Leave it here even if not used.
#define CRYSTAL_FREQ_HZ 12000000

// OS timer settings
#define STM32_ST_IRQ_PRIORITY   2
#define STM32_ST_USE_TIMER      5
#define SYS_TIM_CLK             (Clk.GetTimInputFreq(TIM5))

//  Periphery
#define I2C1_ENABLED            FALSE
#define I2C2_ENABLED            FALSE
#define I2C3_ENABLED            FALSE
#define SIMPLESENSORS_ENABLED   FALSE
#define BUTTONS_ENABLED         FALSE

#define ADC_REQUIRED            FALSE


#define LED_CNT                 150L   // Number of LEDs


#if 1 // ========================== GPIO =======================================
// EXTI
#define INDIVIDUAL_EXTI_IRQ_REQUIRED    FALSE

// UART
#define UART_GPIO       GPIOA
#define UART_TX_PIN     9
#define UART_RX_PIN     10

// Buttons
#define BTN1_PIN        GPIOB, 3
#define BTN2_PIN        GPIOB, 4
#define BTN3_PIN        GPIOB, 5
#define BTN4_PIN        GPIOB, 8

// LCD
#define LCD_SDA         GPIOC, 6
#define LCD_XRES        GPIOC, 7
#define LCD_SCLK        GPIOC, 8
#define LCD_AF          AF8
#define LCD_XCS         GPIOB, 12
#define LCD_BCKLT       { GPIOC, 9, TIM3, 4, invNotInverted, omPushPull, 100 }
#define LCD_PWR         GPIOB, 11

// Neopixel
#define NPX_SPI_A       SPI2
#define NPX_GPIO_A      GPIOC
#define NPX_PIN_A       3
#define NPX_AF_A        AF5

#define NPX_SPI_B       SPI3
#define NPX_GPIO_B      GPIOC
#define NPX_PIN_B       12
#define NPX_AF_B        AF6

// I2C
#define I2C1_GPIO       GPIOB
#define I2C1_SCL        6
#define I2C1_SDA        7

// USB
#define USB_DETECT_PIN  GPIOB, 13
#define USB_DM          GPIOB, 14
#define USB_DP          GPIOB, 15
#define USB_AF          AF10

// Radio: SPI, PGpio, Sck, Miso, Mosi, Cs, Gdo0
#define CC_Setup0       SPI1, GPIOA, 5,6,7, GPIOA,4, GPIOA,3

#endif // GPIO



#if I2C2_ENABLED // ====================== I2C ================================
#define I2C2_BAUDRATE   400000
#define I2C_PILL        i2c2
#endif

#if ADC_REQUIRED // ======================= Inner ADC ==========================
#define ADC_MEAS_PERIOD_MS  2007
// Clock divider: clock is generated from the APB2
#define ADC_CLK_DIVIDER		adcDiv4

// ADC channels
#define ADC_CHNL_BATTERY    10

#define ADC_CHANNELS        { ADC_CHNL_BATTERY, ADC_CHNL_VREFINT }
#define ADC_CHANNEL_CNT     2   // Do not use countof(AdcChannels) as preprocessor does not know what is countof => cannot check
#define ADC_SAMPLE_TIME     ast84Cycles
#define ADC_SAMPLE_CNT      8   // How many times to measure every channel

#define ADC_MAX_SEQ_LEN     16  // 1...16; Const, see ref man
#define ADC_SEQ_LEN         (ADC_SAMPLE_CNT * ADC_CHANNEL_CNT)
#if (ADC_SEQ_LEN > ADC_MAX_SEQ_LEN) || (ADC_SEQ_LEN == 0)
#error "Wrong ADC channel count and sample count"
#endif
#endif

#if 1 // =========================== DMA =======================================
#define STM32_DMA_REQUIRED  TRUE
// ==== Uart ====
// Remap is made automatically if required
#define UART_DMA_TX     STM32_DMA2_STREAM7
#define UART_DMA_RX     STM32_DMA2_STREAM2
#define UART_DMA_CHNL   4
#define UART_DMA_TX_MODE(Chnl) \
                            (STM32_DMA_CR_CHSEL(Chnl) | \
                            DMA_PRIORITY_LOW | \
                            STM32_DMA_CR_MSIZE_BYTE | \
                            STM32_DMA_CR_PSIZE_BYTE | \
                            STM32_DMA_CR_MINC |       /* Memory pointer increase */ \
                            STM32_DMA_CR_DIR_M2P |    /* Direction is memory to peripheral */ \
                            STM32_DMA_CR_TCIE         /* Enable Transmission Complete IRQ */)

#define UART_DMA_RX_MODE(Chnl) \
                            (STM32_DMA_CR_CHSEL((Chnl)) | \
                            DMA_PRIORITY_MEDIUM | \
                            STM32_DMA_CR_MSIZE_BYTE | \
                            STM32_DMA_CR_PSIZE_BYTE | \
                            STM32_DMA_CR_MINC |       /* Memory pointer increase */ \
                            STM32_DMA_CR_DIR_P2M |    /* Direction is peripheral to memory */ \
                            STM32_DMA_CR_CIRC         /* Circular buffer enable */)

#define LCD_DMA         STM32_DMA2_STREAM6  // USART6 TX
#define LCD_DMA_CHNL    5

#define NPX_DMA_A       STM32_DMA1_STREAM4
#define NPX_DMA_CHNL_A  0
#define NPX_DMA_B       STM32_DMA1_STREAM5
#define NPX_DMA_CHNL_B  0

#if I2C1_ENABLED // ==== I2C1 ====
#define I2C1_DMA_TX     STM32_DMA1_STREAM6
#define I2C1_DMA_RX     STM32_DMA1_STREAM5
#endif
#if I2C2_ENABLED // ==== I2C2 ====
#define I2C2_DMA_TX     STM32_DMA1_STREAM7
#define I2C2_DMA_RX     STM32_DMA1_STREAM3
#endif

#if ADC_REQUIRED
#define ADC_DMA         STM32_DMA2_STREAM0
#define ADC_DMA_MODE    STM32_DMA_CR_CHSEL(0) |   /* DMA2 Stream0 Channel0 */ \
                        DMA_PRIORITY_LOW | \
                        STM32_DMA_CR_MSIZE_HWORD | \
                        STM32_DMA_CR_PSIZE_HWORD | \
                        STM32_DMA_CR_MINC |       /* Memory pointer increase */ \
                        STM32_DMA_CR_DIR_P2M |    /* Direction is peripheral to memory */ \
                        STM32_DMA_CR_TCIE         /* Enable Transmission Complete IRQ */
#endif // ADC

#endif // DMA

#if 1 // ========================== USART ======================================
#define PRINTF_FLOAT_EN FALSE
#define UART_TXBUF_SZ   2048
#define UART_RXBUF_SZ   99
#define CMD_UART_PARAMS \
    USART1, UART_GPIO, UART_TX_PIN, UART_GPIO, UART_RX_PIN, \
    UART_DMA_TX, UART_DMA_RX, UART_DMA_TX_MODE(UART_DMA_CHNL), UART_DMA_RX_MODE(UART_DMA_CHNL)

// LCD USART
#define LCD_UART        USART6
#define LCD_UART_SPEED  100000
#define LCD_UART_EN()   rccEnableUSART6(FALSE)

#endif
