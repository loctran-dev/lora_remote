#ifndef BUTTON_H
#define BUTTON_H
#include "stm32f1xx_hal.h"

typedef enum {
    IDLE,
    DEBOUNCE,
    PRESSED,
    LONG_PRESS,
    WAIT_RELEASE
} ButtonState_t;

typedef struct {
    GPIO_TypeDef* GPIOx;
    uint16_t GPIO_Pin;
    ButtonState_t state;
    uint32_t timer;
    uint8_t isPressed; // Flag for application
    uint8_t isLongPressed;
} Button_t;

void Button_UpdateState(Button_t *btn);
#endif
