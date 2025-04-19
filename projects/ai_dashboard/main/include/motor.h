#pragma once
#include <driver/pwm.h>


#define PWM_DUTY_CYCLE            0.3  // PWM_DUTY_CYCLE
#define PWM_MOTOR_CH_3            PWM_ID_3  //PWM channel

/**
 * @brief   init the pwm channel,output pwm wave
 *
 * This API to init the pwm channel,output pwm wave
 *
 */
void motor_open(pwm_chan_t channel);


/**
 * @brief   Stop a PWM channel
 *
 * This API to stop output pwm wave
 *
 */
void motor_close(pwm_chan_t channel);
