#include "hal.h"
#include "MsgQ.h"
#include "kl_lib.h"
#include "Sequences.h"
#include "shell.h"
#include "uart.h"
#include "ws2812b.h"
#include "radio_lvl1.h"
#include "Effects.h"
#include "led.h"

#if 1 // =============== Low level ================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};
void OnCmd(Shell_t *PShell);
void ITask();

static const NeopixelParams_t LedParamsA(NPX_SPI_A, NPX_GPIO_A, NPX_PIN_A, NPX_AF_A, NPX_DMA_A, NPX_DMA_MODE(NPX_DMA_CHNL_A), LED_CNT, npxRGB);
Neopixels_t Leds(&LedParamsA);

static enum LedsState_t {ledsstOff, ledsstPoweringOn, ledsstPoweringOff, ledsstOn} LedsState = ledsstOff;

LedBlinker_t LedBlink{GPIOB, 2, omPushPull};

TmrKL_t TmrCheckRx{MS2ST(4005), evtIdCheckRx, tktPeriodic};
TmrKL_t TmrOff{MS2ST(63000), evtIdOff, tktOneShot};
#endif

void CheckRxTable() {
    RxTable_t& Tbl = Radio.GetRxTable();
//    Printf("Cnt around: %u\r", Tbl.Cnt);
    if(Tbl.Cnt > 0) {
        Printf("Cnt around: %u\r", Tbl.Cnt);
        TmrOff.StartOrRestart();
        // Start if not yet
        if(LedsState == ledsstOff or LedsState == ledsstPoweringOff) {
            LedsState = ledsstPoweringOn;
            Printf("PowerOn\r");
            Effects::PowerOn();
        }
    }
}


int main() {
    // ==== Setup clock ====
    Clk.SetCoreClk(cclk20MHz);
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

    Radio.Init();
    Leds.Init();
    Effects::Init();
    TmrCheckRx.StartOrRestart();

    LedBlink.Init();
    LedBlink.StartOrRestart(lbsqBlink);

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

            case evtIdCheckRx:
                CheckRxTable();
                break;

            case evtIdLedsDone:
                if(LedsState == ledsstPoweringOn) LedsState = ledsstOn;
                else LedsState = ledsstOff;
                Printf("Leds st: %u\r", LedsState);
                break;

            case evtIdOff:
                Printf("PowerOff\r");
                Effects::PowerOff();
                LedsState = ledsstOff;
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

    else if(PCmd->NameIs("SetAll")) {
        Color_t Clr;
        if(PCmd->GetParams<uint8_t>(3, &Clr.R, &Clr.G, &Clr.B) == retvOk) {
            Leds.SetAll(Clr);
            Leds.SetCurrentColors();
            PShell->Ack(retvOk);
        }
        else PShell->Ack(retvCmdError);
    }

    else if(PCmd->NameIs("On"))  Effects::PowerOn();
    else if(PCmd->NameIs("Off")) Effects::PowerOff();

    else if(PCmd->NameIs("Params")) {
        if(PCmd->GetNext<int32_t>(&Params.SpeedMin) != retvOk) return;
        if(PCmd->GetNext<int32_t>(&Params.SpeedMax) != retvOk) return;
        if(PCmd->GetNext<int32_t>(&Params.SizeMin)  != retvOk) return;
        if(PCmd->GetNext<int32_t>(&Params.SizeMax)  != retvOk) return;
        if(PCmd->GetNext<int32_t>(&Params.DelayMin) != retvOk) return;
        if(PCmd->GetNext<int32_t>(&Params.DelayMax) != retvOk) return;
        PShell->Ack(0);
    }

    else PShell->Ack(retvCmdUnknown);
}
#endif
