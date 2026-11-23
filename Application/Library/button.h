#ifndef BUTTON_H
#define BUTTON_H
#include "stm32f1xx_hal.h"

typedef enum Button_State_e
{
    eButton_State_Idle,
    eButton_State_Debounce,
    eButton_State_Pressed,
    eButton_State_LongPress,
    eButton_State_WaitRelease
} Button_State_e;

typedef struct
{
    GPIO_TypeDef *GPIOx;
    uint16_t GPIO_Pin;
    Button_State_e state;
    uint32_t timer;
    uint8_t isPressed; /* Flag for application */
    uint8_t isLongPressed;
} Button_t;

void Button_UpdateState(Button_t *btn);

#endif
