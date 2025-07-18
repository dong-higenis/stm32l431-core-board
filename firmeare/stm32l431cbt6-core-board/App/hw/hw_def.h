#ifndef HW_DEF_H_
#define HW_DEF_H_


#include "bsp.h"


#define _DEF_FIRMWATRE_VERSION    "V250707R1"
#define _DEF_BOARD_NAME           "STM32L431CBT6-CORE"

#define _USE_HW_LED
#define      HW_LED_MAX_CH          1

#define _USE_HW_UART
#define      HW_UART_MAX_CH         1
#define      HW_UART_CH_DEBUG      	_DEF_UART1

#define _USE_HW_LOG
#define      HW_LOG_CH              HW_UART_CH_DEBUG
#define      HW_LOG_BOOT_BUF_MAX    2048
#define      HW_LOG_LIST_BUF_MAX    4096

#define _USE_HW_CLI
#define      HW_CLI_CMD_LIST_MAX    32
#define      HW_CLI_CMD_NAME_MAX    16
#define      HW_CLI_LINE_HIS_MAX    8
#define      HW_CLI_LINE_BUF_MAX    64

#define _USE_HW_CLI_GUI
#define      HW_CLI_GUI_WIDTH       80
#define      HW_CLI_GUI_HEIGHT      24

#define _USE_HW_BUTTON
#define      HW_BUTTON_MAX_CH       1

#define _USE_HW_SWTIMER
#define      HW_SWTIMER_MAX_CH      8

#define _USE_HW_GPIO
#define      HW_GPIO_CH_BTN         0
#define      HW_GPIO_MAX_CH         1


typedef enum
{
  BTN,
  GPIO_PIN_MAX
} GpioPinName_t;


#endif
