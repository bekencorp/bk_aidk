#pragma once



#define countdown_ms 180000 //countdown time
#define COUNTDOWN_NETWORK_PROVISIONING (5*60*1000)	//5 min

void start_countdown(uint32_t time_ms); //create and start oneshot_timer
void stop_countdown();  //close and delete oneshot_timer