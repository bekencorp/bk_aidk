#include <components/log.h>
#include "driver/pwm.h"
#include <os/os.h>
#include "motor.h"
#include "pwm_hal.h"

#include <driver/gpio.h>
#include "gpio_map.h"
#include "driver/pwr_clk.h"

#define TAG "MOTOR"
#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#define PWM_CLOCK_SOURCE (26000000)
#define PWM_FREQ         (1000)


static uint32_t s_period = PWM_CLOCK_SOURCE / PWM_FREQ;

static bk_err_t motor_ldo_power_enable(uint8_t enable)
{
	if (enable) {
		bk_pm_module_vote_ctrl_external_ldo(GPIO_CTRL_LDO_MODULE_MOTOR, MOTOR_LDO_CTRL_GPIO, GPIO_OUTPUT_STATE_HIGH);
		LOGI("MOTOR_LDO_CTRL_GPIO is %d\r\n",MOTOR_LDO_CTRL_GPIO);
	} else {
		bk_pm_module_vote_ctrl_external_ldo(GPIO_CTRL_LDO_MODULE_MOTOR, MOTOR_LDO_CTRL_GPIO, GPIO_OUTPUT_STATE_LOW);
		LOGI("MOTOR_LDO_CTRL_GPIO is %d\r\n",MOTOR_LDO_CTRL_GPIO);
	}

	return BK_OK;
}

static void pwm_init(pwm_chan_t channel, uint32_t period)
{
	pwm_init_config_t init_config = {0};
	init_config.psc = 0,
	init_config.period_cycle = s_period;
	init_config.duty_cycle = period;
	init_config.duty2_cycle = 0;
	init_config.duty3_cycle = 0;
	bk_pwm_init(channel, &init_config);
	bk_pwm_start(channel);
    LOGI("PWM channel is %d\r\n",channel);
}

void motor_close(pwm_chan_t channel){
	bk_pwm_stop(channel);
	motor_ldo_power_enable(0);
	LOGI("motor stop\r\n");
	
}

void motor_open(pwm_chan_t channel){
	pwm_init(channel, PWM_DUTY_CYCLE*s_period);
	motor_ldo_power_enable(1);
	LOGI("motor start \r\n");
}

