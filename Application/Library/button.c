#include "button.h"

/**
 * @brief  Sample the pin and advance the debounce state machine.
 * @param  btn  Button to update.
 * @note   Call every 1 ms from sensorTask.
 */
void Button_UpdateState(Button_t *btn)
{
    uint8_t reading;

    /* Read the button hardware status. */
    reading = (HAL_GPIO_ReadPin(btn->GPIOx, btn->GPIO_Pin) == GPIO_PIN_SET);

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
