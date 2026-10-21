#include "hal_data.h"
#include "motor.h"
#include "tcu.h"

#define PWM_PERIOD 2400000UL

// 서보 PWM: 20ms 주기 기준 3%(최소각)~12%(최대각)
static uint32_t ratio_to_servo(float ratio) {
    return (uint32_t)(PWM_PERIOD * (3.0f + ratio * 0.09f) / 100.0f);
}

// 수동 모드에서 현재 기어 범위로 TPS 클램핑
static float get_motor_ratio(uint8_t gear, float tps) {
    // 기어별 TPS 범위: 1단 0~20%, 2단 20~50%, 3단 50~80%, 4단 80~100%
    const float lo[] = {0, 20, 50, 80};
    const float hi[] = {20, 50, 80, 100};
    if(gear < 1 || gear > 4) return 0.0f;
    uint8_t g = gear - 1;
    if(tps < lo[g]) return lo[g];
    if(tps > hi[g]) return hi[g];
    return tps;
}

void motor_init(void) {
    R_GPT_Open(&g_timer_gpt0_ctrl, &g_timer_gpt0_cfg);  // 서보
    R_GPT_Open(&g_timer_gpt3_ctrl, &g_timer_gpt3_cfg);  // DC
    R_GPT_Start(&g_timer_gpt0_ctrl);
    R_GPT_Start(&g_timer_gpt3_ctrl);
    motor_stop();
}

void motor_stop(void) {
    R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_09_PIN_00, BSP_IO_LEVEL_LOW);
    R_GPT_DutyCycleSet(&g_timer_gpt0_ctrl, ratio_to_servo(0), GPT_IO_PIN_GTIOCA);
    R_GPT_DutyCycleSet(&g_timer_gpt3_ctrl, 0, GPT_IO_PIN_GTIOCA);
}

// 레버/모드/기어/TPS에 따라 서보+DC 모터 제어
void motor_update(tcu_state_t *s, float tps) {
    if(s->lever == LEVER_P) { motor_stop(); return; }

    float r = (s->mode == MODE_AUTO) ? tps : get_motor_ratio(s->gear, tps);

    // 서보 모터(스로틀) 업데이트
    R_GPT_DutyCycleSet(&g_timer_gpt0_ctrl, ratio_to_servo(r), GPT_IO_PIN_GTIOCA);

    if(s->lever == LEVER_N) {
        R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_09_PIN_00, BSP_IO_LEVEL_LOW);
        return;
    }

    // D/R 구동
        R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_09_PIN_00, BSP_IO_LEVEL_HIGH); // Enable

        if(s->lever == LEVER_D) {
            R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_09_PIN_01, BSP_IO_LEVEL_HIGH);
            // H-Bridge: IN1=HIGH 기준, 전위차 r%를 만들기 위해 duty = 100-r
            float fwd_r = 100.0f - r;
            R_GPT_DutyCycleSet(&g_timer_gpt3_ctrl, (uint32_t)(PWM_PERIOD * fwd_r / 100.0f), GPT_IO_PIN_GTIOCA);
        }
        else { // LEVER_R (후진)
            R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_09_PIN_01, BSP_IO_LEVEL_LOW);
            R_GPT_DutyCycleSet(&g_timer_gpt3_ctrl, (uint32_t)(PWM_PERIOD * r / 100.0f), GPT_IO_PIN_GTIOCA);
        }
}
