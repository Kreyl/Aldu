#include "hal.h"
#include "MsgQ.h"
#include "kl_lib.h"
#include "Sequences.h"
#include "shell.h"
#include "uart.h"
#include "lcd1200.h"

#if 1 // =============== Low level ================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};
void OnCmd(Shell_t *PShell);
void ITask();
#endif

int main() {
#if 1 // Low level init
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
    Lcd.Backlight(100);
    Lcd.Printf(0, 0, "Aiya Feanaro!");

    SimpleSensors::Init();
#endif

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

//            case evtIdButtons:
////                Printf("Btn: %u %u\r", Msg.BtnEvtInfo.BtnID, Msg.BtnEvtInfo.Type);
//                if(Msg.BtnEvtInfo.BtnID == 2 and Msg.BtnEvtInfo.Type == beLongPress) {
//                    PowerOff();
//                }
//                break;

            case evtIdEverySecond:
//                Printf("Second\r");
                break;

            default: break;
        } // switch
    } // while true
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
