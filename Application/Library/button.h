/**
 * @file    button.h
 * @brief   Debounced push buttons with click and long-press detection.
 *
 * Button_UpdateState() is polled every millisecond by sensorTask. A press is
 * accepted after 20 ms; releasing it before 1 s sets isPressed, holding it
 * for 1 s sets isLongPressed. systemTask consumes and clears the flags.
 *
 * @defgroup button Buttons
 * @{
 */
#ifndef BUTTON_H
#define BUTTON_H
#include "stm32f1xx_hal.h"

/** @brief Debounce state of a button. */
typedef enum Button_State_e
{
    eButton_State_Idle,       ///< Released.
    eButton_State_Debounce,   ///< Pin went high; waiting 20 ms.
    eButton_State_Pressed,    ///< Held; a release now is a click.
    eButton_State_LongPress,  ///< Not used by Button_UpdateState().
    eButton_State_WaitRelease ///< Long press reported; waiting for release.
} Button_State_e;

/** @brief One button: its pin, debounce state and the flags for the application. */
typedef struct
{
    GPIO_TypeDef *GPIOx;   ///< GPIO port; the pin reads high while pressed.
    uint16_t GPIO_Pin;     ///< GPIO pin.
    Button_State_e state;  ///< Debounce state.
    uint32_t timer;        ///< HAL_GetTick() when the press started [ms].
    uint8_t isPressed;     ///< Click detected; cleared by the application.
    uint8_t isLongPressed; ///< Long press detected; cleared by the application.
} Button_t;

void Button_UpdateState(Button_t *btn);

#endif
/** @} */
