/*
 * main.h
 *
 *  Created on: 18 ����. 2017 �.
 *      Author: Kreyl
 */

#pragma once

#include "kl_lib.h"


enum State_t {stIdle, stHours, stMinutes, stYear, stMonth, stDay};

extern State_t State;
