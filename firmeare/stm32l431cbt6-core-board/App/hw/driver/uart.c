#include "uart.h"
#include "qbuffer.h"
#include "cli.h"
#include "util.h"
#include "gpio.h"
#ifdef _USE_HW_USB
#include "cdc.h"
#endif

#ifdef _USE_HW_UART


#define UART_RX_BUF_LENGTH        1024
#define MAX_BUF_SIZE              100


typedef struct
{
  bool is_open;
  uint32_t baud;

  uint8_t  rx_buf[UART_RX_BUF_LENGTH];
  qbuffer_t qbuffer;
  UART_HandleTypeDef *p_huart;
  DMA_HandleTypeDef  *p_hdma_rx;

  uint32_t rx_cnt;
  uint32_t tx_cnt;
} uart_tbl_t;

typedef enum {
	UART_TYPE_NORMAL,
	UART_TYPE_RS485,
	UART_TYPE_LIN
} uart_type_t;

typedef struct
{
  const char         *p_msg;
  USART_TypeDef      *p_uart;
  UART_HandleTypeDef *p_huart;
  DMA_HandleTypeDef  *p_hdma_rx;
  DMA_HandleTypeDef  *p_hdma_tx;
  uart_type_t        uart_type;
} uart_hw_t;



#ifdef _USE_HW_CLI
static void cliUart(cli_args_t *args);
#endif


static bool is_init = false;

__attribute__((section(".non_cache")))
static uart_tbl_t uart_tbl[UART_MAX_CH];

extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_rx;

const static uart_hw_t uart_hw_tbl[UART_MAX_CH] =
{
  {"USART1   DEBUG   ", USART1,   &huart1,    &hdma_usart1_rx,  NULL, UART_TYPE_NORMAL}
};

bool uartInit(void)
{
  for (int i=0; i<UART_MAX_CH; i++)
  {
    uart_tbl[i].is_open = false;

    switch (i)
    {
      case HW_UART_CH_DEBUG:
        uart_tbl[i].baud = 115200;
        if(uart_hw_tbl[i].uart_type == UART_TYPE_RS485)
        {
        	//	RS485 Disable
        	//HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, GPIO_PIN_SET);
        }
        break;
    }

    uart_tbl[i].rx_cnt = 0;
    uart_tbl[i].tx_cnt = 0;
  }

  is_init = true;

#ifdef _USE_HW_CLI
  cliAdd("uart", cliUart);
#endif

  return true;
}

bool uartDeInit(void)
{
  return true;
}

bool uartIsInit(void)
{
  return is_init;
}

bool uartOpen(uint8_t ch, uint32_t baud)
{
  bool ret = false;
  HAL_StatusTypeDef ret_hal;

  if (ch >= UART_MAX_CH) return false;

  if (uart_tbl[ch].is_open == true && uart_tbl[ch].baud == baud)
  {
    return true;
  }


  switch(ch)
  {
    case HW_UART_CH_DEBUG:
      uart_tbl[ch].baud      = baud;
      uart_tbl[ch].p_huart   = uart_hw_tbl[ch].p_huart;
      uart_tbl[ch].p_hdma_rx = uart_hw_tbl[ch].p_hdma_rx;
      uart_tbl[ch].p_huart->Instance = uart_hw_tbl[ch].p_uart;

      uart_tbl[ch].p_huart->Init.BaudRate       = baud;
      uart_tbl[ch].p_huart->Init.WordLength     = UART_WORDLENGTH_8B;
      uart_tbl[ch].p_huart->Init.StopBits       = UART_STOPBITS_1;
      uart_tbl[ch].p_huart->Init.Parity         = UART_PARITY_NONE;
      uart_tbl[ch].p_huart->Init.Mode           = UART_MODE_TX_RX;
      uart_tbl[ch].p_huart->Init.HwFlowCtl      = UART_HWCONTROL_NONE;
      uart_tbl[ch].p_huart->Init.OverSampling   = UART_OVERSAMPLING_16;
      uart_tbl[ch].p_huart->Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
      uart_tbl[ch].p_huart->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

      qbufferCreate(&uart_tbl[ch].qbuffer, &uart_tbl[ch].rx_buf[0], UART_RX_BUF_LENGTH);

      __HAL_RCC_DMA1_CLK_ENABLE();

      HAL_UART_DeInit(uart_tbl[ch].p_huart);

      if(uart_hw_tbl[ch].uart_type == UART_TYPE_RS485)
      {
        ret_hal = HAL_RS485Ex_Init(uart_tbl[ch].p_huart, UART_DE_POLARITY_HIGH, 0, 0);
      }
      else if(uart_hw_tbl[ch].uart_type == UART_TYPE_LIN)
      {
    	  ret_hal = HAL_LIN_Init(uart_tbl[ch].p_huart, UART_LINBREAKDETECTLENGTH_11B);
      }
      else
      {
        ret_hal = HAL_UART_Init(uart_tbl[ch].p_huart);
      }

      if (ret_hal == HAL_OK)
      {
        ret = true;
        uart_tbl[ch].is_open = true;

        if(HAL_UART_Receive_DMA(uart_tbl[ch].p_huart, (uint8_t *)&uart_tbl[ch].rx_buf[0], UART_RX_BUF_LENGTH) != HAL_OK)
        {
          ret = false;
        }

        uart_tbl[ch].qbuffer.in  = uart_tbl[ch].qbuffer.len - ((DMA_Channel_TypeDef *)uart_tbl[ch].p_huart->hdmarx->Instance)->CNDTR;
        uart_tbl[ch].qbuffer.out = uart_tbl[ch].qbuffer.in;
      }
      break;
  }

  return ret;
}

bool uartClose(uint8_t ch)
{
  if (ch >= UART_MAX_CH) return false;

  if(uart_hw_tbl[ch].uart_type == UART_TYPE_LIN)
  {
  }

  uart_tbl[ch].is_open = false;

  return true;
}

uint32_t uartAvailable(uint8_t ch)
{
  uint32_t ret = 0;

  switch(ch)
  {
    case HW_UART_CH_DEBUG:
    {
      uart_tbl[ch].qbuffer.in = (uart_tbl[ch].qbuffer.len - ((DMA_Channel_TypeDef *)uart_tbl[ch].p_hdma_rx->Instance)->CNDTR);
      ret = qbufferAvailable(&uart_tbl[ch].qbuffer);
    }
    break;
  }

  return ret;
}

bool uartIsOpen(uint8_t ch)
{
  bool ret = false;

  switch(ch)
  {
    case HW_UART_CH_DEBUG:
      ret = uart_tbl[ch].is_open = true;
      break;
  }
  return ret;
}

bool uartFlush(uint8_t ch)
{
  uint32_t pre_time;


  pre_time = millis();
  while(uartAvailable(ch))
  {
    if (millis()-pre_time >= 10)
    {
      break;
    }
    uartRead(ch);
  }

  return true;
}

uint8_t uartRead(uint8_t ch)
{
  uint8_t ret = 0;


  switch(ch)
  {
    case HW_UART_CH_DEBUG:
      qbufferRead(&uart_tbl[ch].qbuffer, &ret, 1);
      break;
  }
  uart_tbl[ch].rx_cnt++;

  return ret;
}

bool uartLinSendBreak(uint8_t ch)
{
   bool ret = false;

  switch(ch)
  {
  case HW_UART_CH_DEBUG:
      if (HAL_LIN_SendBreak(uart_tbl[ch].p_huart) == HAL_OK)
      {
        ret = true;
      }
      break;
  }

  return ret;
}


uint32_t uartWrite(uint8_t ch, uint8_t *p_data, uint32_t length)
{
  uint32_t ret = 0;


  switch(ch)
  {
      case HW_UART_CH_DEBUG:
      if (HAL_UART_Transmit(uart_tbl[ch].p_huart, p_data, length, 100) == HAL_OK)
      {
        ret = length;
      }
      break;
  }
  uart_tbl[ch].tx_cnt += ret;

  return ret;
}

uint32_t uartPrintf(uint8_t ch, const char *fmt, ...)
{
  char buf[256];
  va_list args;
  int len;
  uint32_t ret;

  va_start(args, fmt);
  len = vsnprintf(buf, 256, fmt, args);

  ret = uartWrite(ch, (uint8_t *)buf, len);

  va_end(args);


  return ret;
}

uint32_t uartGetBaud(uint8_t ch)
{
  uint32_t ret = 0;


  if (ch >= UART_MAX_CH)
    return 0;

  ret = uart_tbl[ch].baud;

  return ret;
}

uint32_t uartGetRxCnt(uint8_t ch)
{
  if (ch >= UART_MAX_CH) return 0;

  return uart_tbl[ch].rx_cnt;
}

uint32_t uartGetTxCnt(uint8_t ch)
{
  if (ch >= UART_MAX_CH) return 0;

  return uart_tbl[ch].tx_cnt;
}

#ifdef _USE_HW_CLI
void cliUart(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    for (int i=0; i<UART_MAX_CH; i++)
    {
      cliPrintf("_DEF_UART%d : %s, %d bps\n", i+1, uart_hw_tbl[i].p_msg, uartGetBaud(i));
    }
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "test"))
  {
    uint8_t uart_ch;

    uart_ch = constrain(args->getData(1), 1, UART_MAX_CH) - 1;

    if (uart_ch != cliGetPort())
    {
      uint8_t rx_data;

      while(1)
      {
        if (uartAvailable(uart_ch) > 0)
        {
          rx_data = uartRead(uart_ch);
          cliPrintf("<- _DEF_UART%d RX : 0x%X\n", uart_ch + 1, rx_data);
        }

        if (cliAvailable() > 0)
        {
          rx_data = cliRead();
          if (rx_data == 'q')
          {
            break;
          }
          else
          {
            uartWrite(uart_ch, &rx_data, 1);
            cliPrintf("-> _DEF_UART%d TX : 0x%X\n", uart_ch + 1, rx_data);
          }
        }
      }
    }
    else
    {
      cliPrintf("This is cliPort\n");
    }
    ret = true;
  }
  if (args->argc == 3 && args->isStr(0, "lin") && args->isStr(1, "test"))
  {
	uint8_t uart_ch;
	//sync 0x55, id 0x16(0xD6), data: 0x11, 0x22, 0x33, checksum: 0xC2
  	uint8_t tx_lin_data[6] = {0x55, 0xD6, 0x11, 0x22, 0x33, 0xC2};

	uart_ch = constrain(args->getData(2), 1, UART_MAX_CH) - 1;
  	uartLinSendBreak(uart_ch);
  	uartWrite(uart_ch, tx_lin_data, 6);
	cliPrintf("-> _DEF_UART%d Send Lin Packet : ", uart_ch + 1);
	for(uint8_t i=0;i<6;i++)
	{
		cliPrintf("%02X ", tx_lin_data[i]);
	}
	cliPrintf("\n");
	ret = true;
  }

  if (ret == false)
  {
    cliPrintf("uart info\n");
    cliPrintf("uart test ch[1~%d]\n", HW_UART_MAX_CH);
    cliPrintf("uart lin test ch[1~%d]\n", HW_UART_MAX_CH);
  }
}
#endif


#endif

