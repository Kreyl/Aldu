/*
 * main.h
 *
 *  Created on: 18 сент. 2017 г.
 *      Author: Kreyl
 */

#pragma once

#include "kl_lib.h"


enum State_t {stIdle, stHours, stMinutes, stYear, stMonth, stDay};

extern State_t State;

class Hypertime_t {
public:
    void ConvertFromTime();
    int32_t H, M;
    bool NewH = true, NewM = true;
};
