#include "hal.h"
#include "MsgQ.h"
#include "kl_lib.h"
#include "Sequences.h"
#include "shell.h"
#include "uart.h"
#include "lcd1200.h"
#include "interface.h"

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

    SimpleSensors::Init();

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
                break;

            case evtIdEverySecond:
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

void IndicateNewSecond() {
//    Hypertime.ConvertFromTime();
//    Printf("HyperH: %u; HyperM: %u\r", Hypertime.H, Hypertime.M);
//    ResetColorsToOffState(ClrH, ClrM);

    // Calculate target colors
//    Color_t TargetClrH;
//    Color_t TargetClrM;

    // ==== Process hours ====
//    SetTargetClrH(Hypertime.H, TargetClrH);
//
//    // ==== Process minutes ====
//    if(Hypertime.M == 0) {
//        SetTargetClrM(0, TargetClrM);
//        SetTargetClrM(12, TargetClrM);
//    }
//    else {
//        SetTargetClrM(Hypertime.M, TargetClrM);
//    }
//    WakeMirilli();
}

void Hypertime_t::ConvertFromTime() {
    // Hours
    int32_t FH = Time.Curr.H;
    if(FH > 11) FH -= 12;
    if(H != FH) {
        H = FH;
        NewH = true;
    }
    // Minutes
    int32_t S = Time.Curr.M * 60 + Time.Curr.S;
    int32_t FMin = (S + 150) / 300;    // 300s in one hyperminute (== 5 minutes)
    if(FMin > 11) FMin = 0;
    if(M != FMin) {
        M = FMin;
        NewM = true;
    }

//    int32_t S = Time.Curr.M * 60 + Time.Curr.S;
//    int32_t FMin = S / 150;    // 150s in one hyperminute (== 2.5 minutes)
//    if(M != FMin) {
//        M = FMin;
//        NewM = true;
//    }
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

    else PShell->Ack(retvCmdUnknown);
}
#endif
