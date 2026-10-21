#include "hal_data.h"
#include "tcu.h"
#include "motor.h"
#include "fnd.h"

FSP_CPP_HEADER
void R_BSP_WarmStart(bsp_warm_start_event_t event);
FSP_CPP_FOOTER

static uint32_t last_diag_ms = 0;
volatile uint32_t ms_counter = 0;  // AGT 1ms 카운터
volatile uint32_t led3_timer = 0;
volatile uint32_t led4_timer = 0;
static tcu_state_t tcu = { LEVER_P, MODE_AUTO, 1, false };
volatile int sw_flag[2]={0,};  // SW3=0, SW4=1

// ADC 읽기 함수
static float adc_read_tps(void) {
    uint16_t raw;
    R_ADC_Read(&g_adc0_ctrl, ADC_CHANNEL_0, &raw);
    return (4095-raw) * 100.0f / 4095.0f;
}

// LED 업데이트
static void led_update(tcu_state_t *s) {
    // LED1: D일 때 ON (Low = ON)
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_10_PIN_08,
        (s->lever == LEVER_D) ? BSP_IO_LEVEL_LOW : BSP_IO_LEVEL_HIGH);
    // LED2: AUTO일 때 ON
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_10_PIN_09,
        (s->mode == MODE_AUTO) ? BSP_IO_LEVEL_LOW : BSP_IO_LEVEL_HIGH);
    // LED3/4: 기어 업/다운 표시
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_10_PIN_10,
        (led3_timer > 0) ? BSP_IO_LEVEL_LOW : BSP_IO_LEVEL_HIGH);
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_11_PIN_00,
        (led4_timer > 0) ? BSP_IO_LEVEL_LOW : BSP_IO_LEVEL_HIGH);


}

// UART 진단 메시지 (ERROR 시 1초마다)
static void send_diagnostic(void) {
    if(ms_counter - last_diag_ms < 1000) return;
    last_diag_ms = ms_counter;
    const char lc[] = {'P','N','D','R'};
    static char buf[5];
    buf[0] = lc[tcu.lever];
    buf[1] = (tcu.mode == MODE_AUTO) ? 'A' : 'M';
    buf[2] = (tcu.lever == LEVER_P) ? '-' : (char)('0' + tcu.gear);
    buf[3] = '\r';
    buf[4] = '\n';
    R_SCI_UART_Write(&g_uart0_ctrl, (uint8_t*)buf, 5);
}

// 초기화
static void init_all(void) {
    // 외부 인터럽트 콜백 (SW1~SW4)
    R_ICU_ExternalIrqOpen(&g_external_irq11_ctrl, &g_external_irq11_cfg);
    R_ICU_ExternalIrqEnable(&g_external_irq11_ctrl);
    R_ICU_ExternalIrqOpen(&g_external_irq12_ctrl, &g_external_irq12_cfg);
    R_ICU_ExternalIrqEnable(&g_external_irq12_ctrl);
    R_ICU_ExternalIrqOpen(&g_external_irq13_ctrl, &g_external_irq13_cfg);
    R_ICU_ExternalIrqEnable(&g_external_irq13_ctrl);
    R_ICU_ExternalIrqOpen(&g_external_irq14_ctrl, &g_external_irq14_cfg);
    R_ICU_ExternalIrqEnable(&g_external_irq14_ctrl);
    // AGT: 1ms 주기 타이머 (LED 점멸, UART 타이밍 기준)
    R_AGT_Open(&g_timer_agt0_ctrl, &g_timer_agt0_cfg);
    R_AGT_Start(&g_timer_agt0_ctrl);
    // GPT0: 서보모터 PWM (20ms 주기), GPT3: DC모터 PWM
    motor_init();
    // ADC
    R_ADC_Open(&g_adc0_ctrl, &g_adc0_cfg);
    R_ADC_ScanCfg(&g_adc0_ctrl, &g_adc0_channel_cfg);
    // UART
    R_SCI_UART_Open(&g_uart0_ctrl, &g_uart0_cfg);
}

void hal_entry(void) {
    init_all();
    uint16_t cds_raw = 0;
    float tps = 0.0f;
    adc_status_t status;
    while(true) {
        // ADC 스캔
        R_ADC_ScanStart(&g_adc0_ctrl);
        do{
            R_ADC_StatusGet(&g_adc0_ctrl, &status);
        }while(status.state == ADC_STATE_SCAN_IN_PROGRESS);

        tps = adc_read_tps();
        R_ADC_Read(&g_adc0_ctrl, ADC_CHANNEL_2, &cds_raw);
        tcu.engine_err = (cds_raw >= 800);

        // 자동 변속
        if(tcu.mode == MODE_AUTO) tcu_auto_shift(&tcu, tps);
        // 수동 기어 up
        if(sw_flag[0]) {
            sw_flag[0] = 0;
            tcu_handle_sw3(&tcu, tps);
        }
        // 수동 기어 down
        if(sw_flag[1]) {
            sw_flag[1] = 0;
            tcu_handle_sw4(&tcu, tps);
        }
        // 모터 업데이트
        motor_update(&tcu, tps);

        // FND/LED 업데이트
        fnd_update(&tcu);
        led_update(&tcu);

        // UART 진단 (1초 주기, 오류 시만)
        if(tcu.engine_err) send_diagnostic();
    }
}

// IRQ callback
// SW3/SW4는 메인루프에서 TPS 값이 필요하므로 flag 방식으로 처리
// SW1/SW2는 TPS 불필요하여 IRQ에서 직접 처리
void R_IRQ_Interrupt(external_irq_callback_args_t *p_args) {
    switch(p_args->channel) {
        case 11: tcu_handle_sw1(&tcu); break;
        case 12: tcu_handle_sw2(&tcu); break;
        case 13:
            sw_flag[0] = 1;
            if(tcu.mode == MODE_MANUAL){
                led3_timer=300;    // 300ms 점등
            }
            break;
        case 14:
            sw_flag[1] = 1;
            if(tcu.mode == MODE_MANUAL){
                led4_timer=300;    // 300ms 점등
            }
            break;
    }
}

// AGT callback
void agt_callback(timer_callback_args_t *p_args) {
    if(p_args->event == TIMER_EVENT_CYCLE_END){
        ms_counter++;
        if(led3_timer > 0) led3_timer--;
        if(led4_timer > 0) led4_timer--;
    }

}

// UART callback
void uart_callback(uart_callback_args_t *p_args) {
    (void)p_args; // 수신 기능 필요 시 여기서 처리
}

void R_BSP_WarmStart(bsp_warm_start_event_t event) {
    if(BSP_WARM_START_POST_C == event) {
        R_IOPORT_Open(&g_ioport_ctrl, g_ioport.p_cfg);
    }
}
