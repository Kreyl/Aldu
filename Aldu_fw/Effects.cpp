/*
 * Effects.cpp
 *
 *  Created on: 28 нояб. 2020 г.
 *      Author: layst
 */

#include "Effects.h"
#include "ws2812b.h"
#include "ch.h"
#include <vector>
#include <deque>
#include "MsgQ.h"

extern Neopixels_t Leds;

#define BLADE_BRT_MAX           255

#define GLOBAL_CLR              {0, 18, 0}

#define ON_OFF_SPEED            45


Params_t Params;

enum BladeOnOfEff_t { effOnOffMoveFromHandle, effOnOffMoveToHandle, effOnOffShowBattery };
enum LayerEvtState_t { lesIdle, lesMustSendAndReady, lesMustSendAndNotReady };
enum BladeGlobalEff_t { effOff, effFlaming, effShowBattery };

static Color_t FlaColors[] = {
//        {0, 0, 255},
        {0, 255, 0},
        {0, 255, 0},
        {0, 255, 0},
//        {0, 255, 255},
        {0, 255, 135},
};
#define FLA_CLR_CNT     countof(FlaColors)

static ColorBuf_t LocalClrBuf;

static void SetColorRing(int32_t Indx, Color_t Clr) {
    if(Indx >= LED_CNT) return;
    Leds.ClrBuf[Indx] = Clr;
}

class ParentEff_t {
private:
    virtual_timer_t Tmr;
protected:
    void StartTimerI(systime_t Delay);
    void StartTimer(systime_t Delay) {
        chSysLock();
        StartTimerI(Delay);
        chSysUnlock();
    }
    bool TimerIsStarted() { return chVTIsArmed(&Tmr); }
    void StopTimerI() { chVTResetI(&Tmr); }
public:
    bool IsIdle = true;
    virtual void UpdateI() = 0;
    ParentEff_t() {
        Tmr.func = nullptr; // Init timer
    }
    virtual ~ParentEff_t() {}
};

// Universal Updating callback
static void TmrCallbackUpdate(void *p) {
    chSysLockFromISR();
    ((ParentEff_t*)p)->UpdateI();
    chSysUnlockFromISR();
}

void ParentEff_t::StartTimerI(systime_t Delay_st) {
    chVTSetI(&Tmr, Delay_st, TmrCallbackUpdate, this);
}

// Area of flaming sword
class FlameArea_t {
private:
    systime_t t0;
    int32_t Speed;
public:
    Color_t Clr;
    int32_t Size;
    bool IsIdle;
    int32_t Position; // Lowest LED of Flarea, all others are up
    int32_t CalcShift() {
        systime_t Elapsed = chVTTimeElapsedSinceX(t0);
        return (Speed * ST2MS(Elapsed)) / 1000; // Shift = Speed [LEDs/s] * Time [ms] / 1000
    }
    void RestartTimeMeasurement() { t0 = chVTGetSystemTimeX(); }
    void Init(int32_t ASize, int32_t ASpeed, Color_t AClr) {
        t0 = chVTGetSystemTimeX();
        Speed = ASpeed;
        Size = ASize;
        Clr = AClr;
        IsIdle = false;
        Position = -Size;
    }
    FlameArea_t(int32_t ASize, int32_t ASpeed, Color_t AClr) {
        Init(ASize, ASpeed, AClr);
    }
};

class OnOffLayer_t : public ParentEff_t {
private:
    systime_t IDelay = 0;
public:
    LayerEvtState_t EvtState = lesIdle;
    int32_t Indx = 0, MaxSz = 9; // MaxSz for battery indication only
    int32_t Speed;
    BladeOnOfEff_t Eff;
    bool IsPoweringOn;
    std::vector<uint8_t> BrtBuf;

    void Init() {
        EvtState = lesMustSendAndNotReady; // Send Evt after eff done
        // Calculate delay for selected speed
        IDelay = MS2ST(1000) / Speed;
        if(IDelay == 0) IDelay = 1;
        if(Eff == effOnOffMoveToHandle) Indx = LED_CNT - 1;
        else Indx = 0; // from handle or show battery
        IsIdle = false;
    }

    void UpdateI() {
        switch(Eff) {
            case effOnOffMoveFromHandle:
                if(IsPoweringOn) BrtBuf[Indx] = BLADE_BRT_MAX;
                else BrtBuf[Indx] = 0;
                Indx++;
                if(Indx >= LED_CNT) StopI(); // End of effect
                else StartTimerI(IDelay);
                break;

            case effOnOffMoveToHandle:
                if(IsPoweringOn) BrtBuf[Indx] = BLADE_BRT_MAX;
                else BrtBuf[Indx] = 0;
                Indx--;
                if(Indx < 0) StopI(); // End of effect
                else StartTimerI(IDelay);
                break;

            case effOnOffShowBattery:
                // Check what to do: move when starting indication, on switch off after pause
                if(Indx >= 0) { // Move; Indx used as a flag
                    BrtBuf[Indx] = BLADE_BRT_MAX;
                    Indx++;
                    if(Indx < MaxSz) StartTimerI(IDelay);
                    else { // End of effect, start timer to switch indication off
                        Indx = -1; // flag to indicate need to power off
                        StartTimerI(S2ST(5));
                    }
                }
                else { // Indx<0 => switch off
                    Eff = effOnOffMoveToHandle;
                    EvtState = lesMustSendAndNotReady; // Send Evt after eff done
                    IsPoweringOn = false;
                    Indx = MaxSz - 1;
                }
                break;
        } // switch
    }

    void Draw() {
        if(IsIdle) {
            if(EvtState == lesMustSendAndReady) {
                EvtQMain.SendNowOrExit(EvtMsg_t(evtIdLedsDone));
                EvtState = lesIdle;
            }
        }
        else {
            if(!TimerIsStarted()) StartTimer(IDelay);
        }
    }
    void StopI() {
        IsIdle = true;
        StopTimerI();
        EvtState = lesMustSendAndReady;
    }

    OnOffLayer_t() : ParentEff_t(),
            Speed(7), Eff(effOnOffMoveFromHandle), IsPoweringOn(true) {}
} OnOffLayer;

#if 1 // =========================== Global layer ==============================
class GlobalEffectLayer_t : public ParentEff_t {
private:
    systime_t t0 = 0, Delay_st = 0;
    std::deque<FlameArea_t> Flareas;

    void AddFlarea() {
        // Get flarea params
        int32_t Size, Speed;
        Color_t IClr;
        Size = Params.GetSize();
        Speed = Params.GetSpeed();
        IClr = FlaColors[Random::Generate(0, FLA_CLR_CNT-1)];

        // Check if idle flash exists
        for(FlameArea_t &Fla : Flareas) {
            if(Fla.IsIdle) {
                Fla.Init(Size, Speed, IClr);
                return;
            }
        }
        // No idle flare found
        if(Flareas.size() < 99) { // Do not add new if too many of them
            Flareas.emplace_back(Size, Speed, IClr);
        }
    }

    void IMixIntoBuf(Color_t AClr, int32_t ABrt, int32_t AIndx) {
//        Printf("%d\r", AIndx);
        if(AIndx < 0 or AIndx >= LED_CNT) return;
        AClr.Brt = ABrt;
        ClrBuf[AIndx].MixWith(AClr);
    }
public:
    Color_t Clr;
    int32_t Smooth_st, CurrBrt, TargetBrt;
    BladeGlobalEff_t Eff;
    std::vector<Color_t> ClrBuf;

    void Init() {
        Clr = clBlack;
        if(Eff == effFlaming) {
            Clr = GLOBAL_CLR;
            Delay_st = Params.GetDelay();
            Clr.Brt = 0; // Any flare will prevail
            t0 = chVTGetSystemTimeX(); // for flares
            AddFlarea();
        }
        IsIdle = false;
    }

    void UpdateI() { }

    void Draw() {
        if(IsIdle) return;
        switch(Eff) {
            case effOff:
            case effShowBattery:
                for(Color_t &FClr : ClrBuf) FClr = Clr;
                IsIdle = true;
                break;

            case effFlaming:
                // Fill buffer with main color
                for(Color_t &FClr : ClrBuf) FClr = Clr; // Clr.Brt set to 0 in Init
                // Add new flarea if needed
                if(chVTTimeElapsedSinceX(t0) >= Delay_st) {
                    AddFlarea();
                    // Prepare next delay
                    t0 = chVTGetSystemTimeX();
                    Delay_st = Params.GetDelay();
                }
                // Draw flashes
                for(FlameArea_t &Fla : Flareas) {
                    // Calculate current position
                    int32_t Shift = Fla.CalcShift();
                    if(Shift != 0) {
                        Fla.Position += Shift;
                        Fla.RestartTimeMeasurement();
                    }
                    if(Fla.Position < LED_CNT) { // Draw flarea
                        // Form main pixels
                        for(int i = 0; i <= Fla.Size; i++) {
                            IMixIntoBuf(Fla.Clr, BLADE_BRT_MAX, Fla.Position + i);
                        }
                    }
                    else Fla.IsIdle = true; // Outside of blade, no need to draw
                } // for
                break;

            default: break;
        }
    }

    GlobalEffectLayer_t() : ParentEff_t(),
            Smooth_st(108), CurrBrt(0), TargetBrt(0), Eff(effFlaming) {}
} GlobalLayer;
#endif

#if 1 // ==== Thread ====
static thread_reference_t ThdRef = nullptr;
static THD_WORKING_AREA(waEffThread, 512);
static void EffThread(void *arg) {
    chRegSetThreadName("Eff");
    systime_t Strt = 0;
    while(true) {
        while(chVTTimeElapsedSinceX(Strt) < MS2ST(4)) chThdSleepMilliseconds(2);
        Strt = chVTGetSystemTimeX();
        // Enter sleep if Leds are busy
        chSysLock();
        if(!Leds.TransmitDone) chThdSuspendS(&ThdRef); // Wait Leds done
        chSysUnlock();
        // Send current buf to leds
        Leds.SetCurrentColors();
        // ==== Process effects ====
        // Clear local buf
        std::fill(LocalClrBuf.begin(), LocalClrBuf.end(), clBlack);
        // Process globals
        GlobalLayer.Draw();
        OnOffLayer.Draw();

        // ==== Mix all layers ====
        for(int i=0; i<LED_CNT; i++) { // Iterate pixels
            uint32_t OnOffBrt = OnOffLayer.BrtBuf[i];
            if(OnOffBrt == 0) SetColorRing(i, clBlack); // Switch it off
            else { // Pixel is on
                Color_t ClrFore = LocalClrBuf[i];
                if(ClrFore.Brt == 0) { // No local effect here
                    SetColorRing(i, GlobalLayer.ClrBuf[i]);
                }
                else {
                    Color_t ClrBack = GlobalLayer.ClrBuf[i];
                    SetColorRing(i, Color_t(ClrFore, ClrBack, ClrFore.Brt));
                }
            } // if is on
        } // for
    } // while true
}
#endif

// Npx-DMA-completed callback
static void OnLedsTransmitEnd() {
    chSysLockFromISR();
    if(ThdRef->state == CH_STATE_SUSPENDED) chThdResumeI(&ThdRef, MSG_OK);
    chSysUnlockFromISR();
}

namespace Effects {

void Init() {
    OnOffLayer.BrtBuf.resize(LED_CNT);
    GlobalLayer.ClrBuf.resize(LED_CNT);

    Leds.OnTransmitEnd = OnLedsTransmitEnd;

    chThdCreateStatic(waEffThread, sizeof(waEffThread), NORMALPRIO, (tfunc_t)EffThread, NULL);
    Leds.SetCurrentColors();
}

void PowerOn() {
    GlobalLayer.Eff = effFlaming;
    GlobalLayer.Init();
    chSysLock();
    OnOffLayer.StopI();
    OnOffLayer.Speed = ON_OFF_SPEED;
    OnOffLayer.Eff = effOnOffMoveFromHandle;
    OnOffLayer.IsPoweringOn = true;
    OnOffLayer.Init();
    chSysUnlock();
}

void PowerOff() {
    OnOffLayer.Speed = ON_OFF_SPEED;
    OnOffLayer.Eff = effOnOffMoveToHandle;
    OnOffLayer.IsPoweringOn = false;
    OnOffLayer.Init();
}

} // namespace
