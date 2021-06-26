#include "hal.h"
#include "MsgQ.h"
#include "kl_lib.h"
#include "Sequences.h"
#include "shell.h"
#include "uart.h"
#include "lcd1200.h"
#include "interface.h"
#include "sk6812.h"
#include "IntelLedEffs.h"

#if 1 // =============== Low level ================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};
void OnCmd(Shell_t *PShell);
void ITask();

Interface_t Interface;
State_t State = stIdle;
bool DateTimeHasChanged = false;

TmrKL_t TmrMenu {MS2ST(9999), evtIdMenuTimeout, tktOneShot};
enum Btns_t {btnUp=0, btnDown=1, btnPlus=2, btnMinus=3};

static void MenuHandler(Btns_t Btn);
static void EnterIdle();
static void IndicateNewSecond();

static const NeopixelParams_t LedParamsA(NPX_SPI_A, NPX_GPIO_A, NPX_PIN_A, NPX_AF_A, NPX_DMA_A, NPX_DMA_MODE(NPX_DMA_CHNL_A));
static const NeopixelParams_t LedParamsB(NPX_SPI_B, NPX_GPIO_B, NPX_PIN_B, NPX_AF_B, NPX_DMA_B, NPX_DMA_MODE(NPX_DMA_CHNL_B));
Neopixels_t NpxA(&LedParamsA);
Neopixels_t NpxB(&LedParamsB);
Effects_t BandA(&NpxA);
Effects_t BandB(&NpxB);

static uint8_t TimeToBrightness(int32_t t, int32_t Offset);
#endif

int main() {
    // ==== Setup clock ====
    Clk.SetCoreClk(cclk16MHz);
    Clk.UpdateFreqValues();

    // ==== Init OS ====
    halInit();
    chSysInit();

    // ==== Init Hard & Soft ====
    EvtQMain.Init();
    Uart.Init();
    Uart.StartRx();
    Printf("\r%S %S\r\n", APP_NAME, XSTRINGIFY(BUILD_TIME));
    Clk.PrintFreqs();

    PinSetupOut(LCD_PWR, omPushPull);
    PinSetHi(LCD_PWR);
    chThdSleepMilliseconds(18);
    Lcd.Init();

    Interface.Reset();
    EnterIdle();
    BackupSpc::EnableAccess();
    Time.Init();

    // Be fast: 8 times faster
    chSysLock();
    Rtc::DisableWriteProtection();
    Rtc::EnterInitMode();
    // Program both the prescaler factors
    RTC->PRER = (15UL << 16) | (0xFFUL);  // async pre = 16, sync = 256 => 32768->8. I.e. 8 seconds within 1 real
    Rtc::ExitInitMode();
    Rtc::EnableWriteProtection();
    chSysUnlock();

    SimpleSensors::Init();

    // Leds
    NpxA.Init();
    NpxB.Init();
    CommonEffectsInit();

    // ==== Main cycle ====
    ITask();
}

__noreturn
void ITask() {
    while(true) {
        EvtMsg_t Msg = EvtQMain.Fetch(TIME_INFINITE);
//        Printf("Msg.ID %u\r", Msg.ID);
        switch(Msg.ID) {
            case evtIdShellCmd:
                OnCmd((Shell_t*)Msg.Ptr);
                ((Shell_t*)Msg.Ptr)->SignalCmdProcessed();
                break;

            case evtIdButtons:
//                Printf("Btn %u\r", Msg.BtnEvtInfo.BtnID);
                MenuHandler((Btns_t)Msg.BtnEvtInfo.BtnID);
//                BandA.AllTogetherSmoothly((Color_t){0,255,0,0}, 720);
                break;

            case evtIdEverySecond:
//                BandB.AllTogetherNow((Color_t){0,255,0,0});
                if(State == stIdle) {
                    Time.GetDateTime();
//                    Time.Curr.Print();
                    Interface.DisplayDateTime();
                }
                IndicateNewSecond();
                break;

            case evtIdMenuTimeout:
                EnterIdle();
                break;

            default: break;
        } // switch
    } // while true
}

void EnterIdle() {
    State = stIdle;
    TmrMenu.Stop();
    // Save time if changed
    if(DateTimeHasChanged) {
        Time.SetDateTime();
        IndicateNewSecond();
    }
    Interface.DisplayDateTime();
    Lcd.Backlight(0);
}

#define ALDU_T1     ((int32_t)(3600 * 3.5))
#define ALDU_T2     ((int32_t)(3600 * 7))
#define ALDU_T3     ((int32_t)(3600 * 12))
#define ALDU_T4     ((int32_t)(3600 * 15.5))
#define ALDU_T5     ((int32_t)(3600 * 19))
#define TOP_BRT     255L

uint8_t TimeToBrightness(int32_t t, int32_t Offset) {
    int32_t Brt;
    // Calculate shifted time
    t = t - Offset;
    if(t < 0) t += 86400;
    if(t < ALDU_T1) {
        // Fade in
        Brt = (TOP_BRT * t) / ALDU_T1;
    }
    else if(t < ALDU_T2) {
        // Fade out
        t -= ALDU_T1;
        Brt = - ((TOP_BRT * t) / ALDU_T1) + TOP_BRT;
    }
    else if(t < ALDU_T3) {
        // Off
        Brt = 0;
    }
    else if(t < ALDU_T4) {
        // Fade in
        t -= ALDU_T3;
        Brt = (TOP_BRT * t) / ALDU_T1;
    }
    else if(t < ALDU_T5) {
        // Fade out
        t -= ALDU_T4;
        Brt = - ((TOP_BRT * t) / ALDU_T1) + TOP_BRT;
    }
    else {
        // Off
        Brt = 0;
    }
    return (uint8_t)Brt;
}

void IndicateNewSecond() {
    int32_t t = Time.Curr.S + Time.Curr.M * 60 + Time.Curr.H * 3600;
    uint8_t Brt;
    Color_t Clr{0, 0, 0, 0};
    // Telperion
    Brt = TimeToBrightness(t, (3600 * 0));
//        Printf("Tlp: %u; ", BrtTlp);
    Clr.W = Brt;
    BandA.AllTogetherNow(Clr);
    // Laurelin
    Brt = TimeToBrightness(t, (3600 * 6));
//        Printf("Lau: %u\r", Brt);
    Clr.W = 0;
    Clr.R = Brt;
    Clr.G = Brt;
    BandB.AllTogetherNow(Clr);
}

void MenuHandler(Btns_t Btn) {
    // Switch backlight on
    Lcd.Backlight(100);
    TmrMenu.StartOrRestart();
    // Process menu
    switch(State) {
        case stIdle:
            switch(Btn) {
                case btnDown:
                    State = stHours;
                    Interface.DisplayDateTime();
                    break;
                case btnUp:
                    State = stDay;
                    Interface.DisplayDateTime();
                    break;
                default: break; // do not react on +-
            }
            break;

#if 1 // ==== Time ====
        case stMinutes:
            switch(Btn) {
                case btnDown: State = stYear; break;
                case btnUp: State = stHours; break;
                case btnPlus:  Time.Curr.IncM(); Time.Curr.S = 0; DateTimeHasChanged = true; break;
                case btnMinus: Time.Curr.DecM(); Time.Curr.S = 0; DateTimeHasChanged = true; break;
            }
            Interface.DisplayDateTime();
            break;

        case stHours:
            switch(Btn) {
                case btnDown: State = stMinutes; break;
                case btnUp: EnterIdle(); break;
                case btnPlus:  Time.Curr.IncH(); DateTimeHasChanged = true; break;
                case btnMinus: Time.Curr.DecH(); DateTimeHasChanged = true; break;
            }
            Interface.DisplayDateTime();
            break;
#endif

#if 1 // ==== Date ====
        case stDay:
            switch(Btn) {
                case btnDown: EnterIdle(); break;
                case btnUp: State = stMonth; break;
                case btnPlus:  Time.Curr.IncDay(); DateTimeHasChanged = true; break;
                case btnMinus: Time.Curr.DecDay(); DateTimeHasChanged = true; break;
            }
            Interface.DisplayDateTime();
            break;

        case stMonth:
            switch(Btn) {
                case btnDown: State = stDay; break;
                case btnUp: State = stYear; break;
                case btnPlus:  Time.Curr.IncMonth(); DateTimeHasChanged = true; break;
                case btnMinus: Time.Curr.DecMonth(); DateTimeHasChanged = true; break;
            }
            Interface.DisplayDateTime();
            break;

        case stYear:
            switch(Btn) {
                case btnDown: State = stMonth; break;
                case btnUp: State = stMinutes; break;
                case btnPlus:  Time.Curr.IncYear(); DateTimeHasChanged = true; break;
                case btnMinus: Time.Curr.DecYear(); DateTimeHasChanged = true; break;
            }
            Interface.DisplayDateTime();
            break;
#endif
            default: break;
    } // switch state
}


#if 1 // ======================= Command processing ============================
void OnCmd(Shell_t *PShell) {
    Cmd_t *PCmd = &PShell->Cmd;
//    Printf("%S  ", PCmd->Name);

    // Handle command
    if(PCmd->NameIs("Ping")) PShell->Ack(retvOk);
    else if(PCmd->NameIs("Version")) PShell->Printf("%S %S\r", APP_NAME, XSTRINGIFY(BUILD_TIME));

//    else if(PCmd->NameIs("GetBat")) { PShell->Printf("Battery: %u\r", Audio.GetBatteryVmv()); }

    else if(PCmd->NameIs("SetTime")) {
        DateTime_t dt = Time.Curr;
        if(PCmd->GetNext<int32_t>(&dt.H) != retvOk) return;
        if(PCmd->GetNext<int32_t>(&dt.M) != retvOk) return;
        Time.Curr = dt;
        Time.SetDateTime();
        IndicateNewSecond();
        PShell->Ack(retvOk);
    }

    else if(PCmd->NameIs("Fast")) {
        Time.BeFast();
        PShell->Ack(retvOk);
    }
    else if(PCmd->NameIs("Norm")) {
        Time.BeNormal();
        PShell->Ack(retvOk);
    }

    else PShell->Ack(retvCmdUnknown);
}
#endif
