#include "button.h"

void Button_UpdateState(Button_t *btn)
{
    /* Read the button hardware status. */
    uint8_t reading = (HAL_GPIO_ReadPin(btn->GPIOx, btn->GPIO_Pin) == GPIO_PIN_SET);

    switch (btn->state)
    {
        case eButton_State_Idle:
            if (reading)
            {
                btn->state = eButton_State_Debounce;
                btn->timer = HAL_GetTick();
            }
            break;

        case eButton_State_Debounce:
            if (HAL_GetTick() - btn->timer >= 20) /* debounce 20 ms */
            {
                if (reading)
                {
                    btn->state = eButton_State_Pressed;
                }
                else
                {
                    btn->state = eButton_State_Idle;
                }
            }
            break;

        case eButton_State_Pressed:
            if (!reading) /* Released: short press */
            {
                btn->isPressed = 1;
                btn->state = eButton_State_Idle;
            }
            else if (HAL_GetTick() - btn->timer >= 1000) /* Long press > 1 s */
            {
                btn->isLongPressed = 1;
                btn->state = eButton_State_WaitRelease;
            }
            break;

        case eButton_State_WaitRelease:
            if (!reading)
            {
                btn->state = eButton_State_Idle;
            }
            break;

        default:
            break;
    }
}
