/*
 * interface.h
 *
 *  Created on: 22 марта 2015 г.
 *      Author: Kreyl
 */

#pragma once

#include "lcd1200.h"
#include "kl_time.h"
#include "main.h"

class Interface_t {
public:
    void Reset() {
        Lcd.Printf(0, 0, "Время:  00:00:00");
        Lcd.Printf(0, 1, "Дата: 0000/00/00");
    }

    void DisplayDateTime() {
        if(State == stHours) Lcd.PrintfInverted(8,  0, "%02u", Time.Curr.H);
        else Lcd.Printf(8,  0, "%02u", Time.Curr.H);
        if(State == stMinutes) Lcd.PrintfInverted(11, 0, "%02u", Time.Curr.M);
        else Lcd.Printf(11, 0, "%02u", Time.Curr.M);
        Lcd.Printf(14, 0, "%02u", Time.Curr.S);  // do not touch seconds
        if(State == stYear) Lcd.PrintfInverted(6,  1, "%04u", Time.Curr.Year);
        else Lcd.Printf(6,  1, "%04u", Time.Curr.Year);
        if(State == stMonth) Lcd.PrintfInverted(11, 1, "%02u", Time.Curr.Month);
        else Lcd.Printf(11, 1, "%02u", Time.Curr.Month);
        if(State == stDay) Lcd.PrintfInverted(14, 1, "%02u", Time.Curr.Day);
        else Lcd.Printf(14, 1, "%02u", Time.Curr.Day);
    }

    void Error(const char* msg) { Lcd.PrintfInverted(0, 0, "%S", msg); }
};

extern Interface_t Interface;
