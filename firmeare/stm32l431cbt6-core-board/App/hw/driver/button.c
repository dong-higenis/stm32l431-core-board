#include "gpio.h"
#include "button.h"
#include "cli.h"
#include "swtimer.h"


#ifdef _USE_HW_BUTTON

#define BUTTON_EVENT_MAX          8

typedef struct
{
  bool        pressed;
  bool        pressed_event;
  uint16_t    pressed_cnt;
  uint32_t    pressed_start_time;
  uint32_t    pressed_end_time;

  bool        released;
  bool        released_event;
  uint32_t    released_start_time;
  uint32_t    released_end_time;

  bool        repeat_update;
  uint32_t    repeat_cnt;
  uint32_t    repeat_time_detect;
  uint32_t    repeat_time_delay;
  uint32_t    repeat_time;
} button_t;

typedef struct
{
  uint8_t      gpio_ch;  
  const char   *p_name;
} button_pin_t;


#ifdef _USE_HW_CLI
static void cliButton(cli_args_t *args);
#endif

static void buttonISR(void *arg);
static bool buttonGetPin(uint8_t ch);

static button_pin_t button_pin[BUTTON_MAX_CH] =
{
    { HW_GPIO_CH_BTN, "BTN"},  // 0. BTN
};

static button_t button_tbl[BUTTON_MAX_CH];
static bool is_enable = true;
static uint16_t event_cnt = 0;
static uint8_t event_level = 5;
static button_event_t *event_tbl[BUTTON_EVENT_MAX];


bool buttonInit(void)
{
  bool ret = true;

  for (int i=0; i<BUTTON_MAX_CH; i++)
  {
    button_tbl[i].pressed_cnt    = 0;
    button_tbl[i].pressed        = 0;
    button_tbl[i].released       = 0;
    button_tbl[i].released_event = 0;

    button_tbl[i].repeat_cnt         = 0;
    button_tbl[i].repeat_time_detect = 60;
    button_tbl[i].repeat_time_delay  = 250;
    button_tbl[i].repeat_time        = 200;

    button_tbl[i].repeat_update = false;
  }

  for (int i=0; i<BUTTON_EVENT_MAX; i++)
  {
    event_tbl[i] = NULL;
  }

  swtimer_handle_t timer_ch;
  timer_ch = swtimerGetHandle();
  if (timer_ch >= 0)
  {
    swtimerSet(timer_ch, 10, LOOP_TIME, buttonISR, NULL);
    swtimerStart(timer_ch);
  }
  else
  {
    logPrintf("[NG] buttonInit()\n     swtimerGetHandle()\n");
  }

#ifdef _USE_HW_CLI
  cliAdd("button", cliButton);
#endif

  return ret;
}

bool buttonEventInit(button_event_t *p_event, uint8_t level)
{
  bool ret = false;

  for (int i=0; i<BUTTON_EVENT_MAX; i++)
  {
    if (event_tbl[i] == NULL)
    {
      memset(p_event, 0, sizeof(button_event_t));

      event_tbl[i] = p_event;
      p_event->level = level;
      p_event->index = i;
      event_cnt++;

      p_event->is_init = true;
      ret = true;
      break;
    }
  }

  return ret;
}

bool buttonEventRemove(button_event_t *p_event)
{
  bool ret = false;

  for (int i=0; i<BUTTON_EVENT_MAX; i++)
  {
    if (event_tbl[i] == p_event)
    {
      __disable_irq();
      event_tbl[i] = NULL;
      if (event_cnt > 0)
      {
        event_cnt--;
      }
      __enable_irq();
      ret = true;
      break;
    }
  }
  return ret;
}

void buttonISR(void *arg)
{
  uint32_t repeat_time;

  for (int i=0; i<BUTTON_MAX_CH; i++)
  {
    if (buttonGetPin(i) == true)
    {
      if (button_tbl[i].pressed == false)
      {
        button_tbl[i].pressed_event = true;
        button_tbl[i].pressed_start_time = millis();

        for (int e_i=0; e_i<event_cnt; e_i++)
        {
          if (event_tbl[e_i]->level <= event_level)
            event_tbl[e_i]->pressed_event[i] = true;
          else
            event_tbl[e_i]->pressed_event[i] = false;
        }
      }

      button_tbl[i].pressed = true;
      button_tbl[i].pressed_cnt++;

      if (button_tbl[i].repeat_cnt == 0)
      {
        repeat_time = button_tbl[i].repeat_time_detect;
      }
      else if (button_tbl[i].repeat_cnt == 1)
      {
        repeat_time = button_tbl[i].repeat_time_delay;
      }
      else
      {
        repeat_time = button_tbl[i].repeat_time;
      }
      if (button_tbl[i].pressed_cnt >= repeat_time)
      {
        button_tbl[i].pressed_cnt = 0;
        button_tbl[i].repeat_cnt++;
        button_tbl[i].repeat_update = true;

        for (int e_i=0; e_i<BUTTON_EVENT_MAX; e_i++)
        {
          if (event_tbl[e_i] != NULL)
          {
            if (event_tbl[e_i]->level <= event_level)
              event_tbl[e_i]->repeat_event[i] = true;
            else
              event_tbl[e_i]->repeat_event[i] = false;
          }
        }
      }

      button_tbl[i].pressed_end_time = millis();

      button_tbl[i].released = false;
    }
    else
    {
      if (button_tbl[i].pressed == true)
      {
        button_tbl[i].released_event = true;
        button_tbl[i].released_start_time = millis();

        for (int e_i=0; e_i<BUTTON_EVENT_MAX; e_i++)
        {
          if (event_tbl[e_i] != NULL)
          {
            if (event_tbl[e_i]->level <= event_level)
              event_tbl[e_i]->released_event[i] = true;
            else
              event_tbl[e_i]->released_event[i] = false;
          }
        }
      }

      button_tbl[i].pressed  = false;
      button_tbl[i].released = true;
      button_tbl[i].repeat_cnt = 0;
      button_tbl[i].repeat_update = false;

      button_tbl[i].released_end_time = millis();
    }
  }
}

//bool gpioPinRead(uint8_t ch);

bool buttonGetPin(uint8_t ch)
{
  bool ret = false;

  if (ch >= BUTTON_MAX_CH)
  {
    return false;
  }
  
  if (gpioPinRead(button_pin[ch].gpio_ch))
  {
    ret = true;
  }

  return ret;
}

void buttonClear(void)
{
  for (int i=0; i<BUTTON_MAX_CH; i++)
  {
    button_tbl[i].pressed_start_time    = 0;
    button_tbl[i].pressed_end_time      = 0;
    button_tbl[i].released_start_time   = 0;
    button_tbl[i].released_end_time     = 0;

    button_tbl[i].pressed_event = false;
    button_tbl[i].released_event = false;
  }
}

void buttonEnable(bool enable)
{
  is_enable = enable;
}

bool buttonGetPressed(uint8_t ch)
{
  if (ch >= BUTTON_MAX_CH || is_enable == false)
  {
    return false;
  }

  return button_tbl[ch].pressed;
}

uint32_t buttonGetData(void)
{
  uint32_t ret = 0;


  for (int i=0; i<BUTTON_MAX_CH; i++)
  {
    ret |= (buttonGetPressed(i)<<i);
  }

  return ret;
}


uint8_t  buttonGetPressedCount(void)
{
  uint32_t i;
  uint8_t ret = 0;

  for (i=0; i<BUTTON_MAX_CH; i++)
  {
    if (buttonGetPressed(i) == true)
    {
      ret++;
    }
  }

  return ret;
}

void buttonSetRepeatTime(uint8_t ch, uint32_t detect_ms, uint32_t repeat_delay_ms, uint32_t repeat_ms)
{
  if (ch >= BUTTON_MAX_CH || is_enable == false) return;

  button_tbl[ch].repeat_update = false;
  button_tbl[ch].repeat_cnt = 0;
  button_tbl[ch].pressed_cnt = 0;

  button_tbl[ch].repeat_time_detect = detect_ms;
  button_tbl[ch].repeat_time_delay  = repeat_delay_ms;
  button_tbl[ch].repeat_time        = repeat_ms;
}

uint32_t buttonGetRepeatCount(uint8_t ch)
{
  volatile uint32_t ret = 0;

  if (ch >= BUTTON_MAX_CH || is_enable == false) return 0;

  ret = button_tbl[ch].repeat_cnt;

  return ret;
}

uint32_t buttonGetPressedTime(uint8_t ch)
{
  volatile uint32_t ret;


  if (ch >= BUTTON_MAX_CH || is_enable == false) return 0;


  ret = button_tbl[ch].pressed_end_time - button_tbl[ch].pressed_start_time;

  return ret;
}


bool buttonGetReleased(uint8_t ch)
{
  bool ret;


  if (ch >= BUTTON_MAX_CH || is_enable == false) return false;

  ret = button_tbl[ch].released;

  return ret;
}

uint32_t buttonGetReleasedTime(uint8_t ch)
{
  volatile uint32_t ret;


  if (ch >= BUTTON_MAX_CH || is_enable == false) return 0;


  ret = button_tbl[ch].released_end_time - button_tbl[ch].released_start_time;

  return ret;
}

bool buttonEventClear(button_event_t *p_event)
{
  bool ret = true;


  if (is_enable == false) return false;
  if (p_event->is_init != true) return false;

  for (int i=0; i<BUTTON_MAX_CH; i++)
  {
    p_event->pressed_event[i] = false;
    p_event->released_event[i] = false;
  }
  return ret;
}

bool buttonEventGetReleased(button_event_t *p_event, uint8_t ch)
{
  bool ret;


  if (ch >= BUTTON_MAX_CH || is_enable == false) return false;
  if (p_event->is_init != true) return false;


  ret = p_event->released_event[ch];
  p_event->released_event[ch] = false;

  return ret;
}

bool buttonEventGetPressed(button_event_t *p_event, uint8_t ch)
{
  bool ret;


  if (ch >= BUTTON_MAX_CH || is_enable == false) return false;
  if (p_event->is_init != true) return false;


  ret = p_event->pressed_event[ch];
  p_event->pressed_event[ch] = false;

  return ret;
}

uint32_t buttonEventGetRepeat(button_event_t *p_event, uint8_t ch)
{
  volatile uint32_t ret = 0;

  if (ch >= BUTTON_MAX_CH || is_enable == false) return 0;
  if (p_event->is_init != true) return false;

  if (p_event->repeat_event[ch])
  {
    p_event->repeat_event[ch] = false;
    ret = button_tbl[ch].repeat_cnt;
  }

  return ret;
}

#ifdef _USE_HW_CLI
void cliButton(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    for (int i=0; i<BUTTON_MAX_CH; i++)
    {
      cliPrintf("%-12s pin %d\n", button_pin[i].p_name, gpioPinRead(button_pin[i].gpio_ch));
    }
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "show"))
  {
    while(cliKeepLoop())
    {
      for (int i=0; i<BUTTON_MAX_CH; i++)
      {
        cliPrintf("%d", buttonGetPressed(i));
      }
      delay(50);
      cliPrintf("\r");
    }
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "time"))
  {
    uint8_t ch;

    ch = (uint8_t)args->getData(1);
    ch = constrain(ch, 0, BUTTON_MAX_CH-1);

    while(cliKeepLoop())
    {
      for (int i=0; i<BUTTON_MAX_CH; i++)
      {
        if(buttonGetPressed(i))
        {
          cliPrintf("%-12s, Time :  %d ms\n", button_pin[i].p_name, buttonGetPressedTime(i));
        }
      }
      delay(10);
    }
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("button info\n");
    cliPrintf("button show\n");
    cliPrintf("button time\n", BUTTON_MAX_CH);
  }
}
#endif


#endif
