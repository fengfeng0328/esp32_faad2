#ifndef TL_I2S_HAL_H
#define TL_I2S_HAL_H

#include "driver/i2c.h"
#include "driver/i2s.h"

void audio_recorder_AC101_init_16KHZ_16BIT_1CHANNEL();
void audio_recorder_AC101_init_44KHZ_16BIT_2CHANNEL();

void i2s_uinit(i2c_port_t i2c_num, i2s_port_t i2s_num);

#endif
