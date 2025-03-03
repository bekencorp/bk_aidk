#ifndef __AGORA_LED_BLINK_H__
#define __AGORA_LED_BLINK_H__

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>

#define BLINK_INTERVAL_MS    500 // blinking time interval
typedef enum {
    LED_MODE_OFF = 0, //off 
    LED_MODE_ON,      //on
    LED_MODE_BLINK    //blinking
} LedMode;

//led init 
int led_service_init(uint8_t led_id,uint32_t blink_interval_ms);

/*According to the input led_id, set the LED working mode, which has three states: always on, 
off, and blinking. The blinking time is controlled by BLINK_INTERVAL_MS*/
void led_set_mode(uint8_t led_id,LedMode mode);



#ifdef __cplusplus
}
#endif
#endif 