#include "button.h"

void Button_UpdateState(Button_t *btn) {
	// Read the Button hardware status
    uint8_t reading = (HAL_GPIO_ReadPin(btn->GPIOx, btn->GPIO_Pin) == GPIO_PIN_SET);

    switch (btn->state) {
        case IDLE:
            if (reading) {
                btn->state = DEBOUNCE;
                btn->timer = HAL_GetTick();
            }
            break;

        case DEBOUNCE:
            if (HAL_GetTick() - btn->timer >= 20) { // debounce 20ms
                if (reading) {
                    btn->state = PRESSED;
                } else {
                    btn->state = IDLE;
                }
            }
            break;

        case PRESSED:
            if (!reading) { //Pressed
                btn->isPressed = 1; 
                btn->state = IDLE;
            } else if (HAL_GetTick() - btn->timer >= 1000) { // Long pressed > 1s
                btn->isLongPressed = 1;
                btn->state = WAIT_RELEASE;
            }
            break;

        case WAIT_RELEASE:
            if (!reading) {
                btn->state = IDLE;
            }
            break;
				default:
					break;
    }
}

