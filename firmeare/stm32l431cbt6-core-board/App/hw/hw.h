#ifndef HW_H_
#define HW_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"

#include "led.h"
#include "uart.h"
#include "log.h"
#include "gpio.h"
#include "cli.h"
#include "cli_gui.h"
#include "swtimer.h"
#include "button.h"


bool hwInit(void);


#ifdef __cplusplus
}
#endif

#endif
