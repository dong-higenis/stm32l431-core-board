#ifndef AP_H_
#define AP_H_


#include "ap_def.h"

#define CAN_ID_LC_STATUS 0x110
//#define CAN_ID_LED_SET   (0x200)
//#define CAN_ID_SERVO_SET (0x201)

void apInit(void);
void apMain(void);


#endif
