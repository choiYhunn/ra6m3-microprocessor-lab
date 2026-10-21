#include "motor.h"

/* ───────────────────────────────────────────────
   PWM 주기 카운트
   GPT0(Servo): 20ms 주기, PCLK=120MHz → 2,400,000 counts
   GPT3(DC모터): 동일 주기 기준 사용
─────────────────────────────────────────────── */
#define PWM_PERIOD_COUNTS  2400000UL

/* ───────────────────────────────────────────────
   서보 각도 → 듀티 카운트 변환
   0°  = Duty 3%  = 0.6ms / 20ms → 72,000 counts
   180° = Duty 12% = 2.4ms / 20ms → 288,000 counts
─────────────────────────────────────────────── */
static uint32_t servo_duty(uint8_t deg) {
    // 0~180° 선형 보간: 3% ~ 12%
    float ratio = 3.0f + (deg / 180.0f) * 9.0f; // 3% ~ 12%
    return (uint32_t)(PWM_PERIOD_COUNTS * ratio / 100.0f);
}

/* ───────────────────────────────────────────────
   초기화: GPT0(Servo), GPT3(DC모터) 시작
─────────────────────────────────────────────── */
void motor_init(void) {
    R_GPT_Open(&g_timer_gpt0_ctrl, &g_timer_gpt0_cfg);  // 서보
    R_GPT_Open(&g_timer_gpt3_ctrl, &g_timer_gpt3_cfg);  // DC모터
    R_GPT_Start(&g_timer_gpt0_ctrl);
    R_GPT_Start(&g_timer_gpt3_ctrl);

    // 초기 상태: 서보 0°(닫힘), DC모터 정지
    servo_set_angle(0);
    dc_motor_stop();
}

/* ───────────────────────────────────────────────
   DC 모터 구동
   UP  → Direction HIGH, Duty = 50% → (100-50)% = 50%
   DOWN → Direction LOW,  Duty = 50%
─────────────────────────────────────────────── */
void dc_motor_start(Direction_t dir) {
    R_IOPORT_PinWrite(&g_ioport_ctrl, MOTOR_ENABLE_PIN, BSP_IO_LEVEL_HIGH); // Enable

    if (dir == DIR_UP) {
        // Direction HIGH: H-Bridge에서 CW 회전
        R_IOPORT_PinWrite(&g_ioport_ctrl, MOTOR_DIR_PIN, BSP_IO_LEVEL_HIGH);
        // Direction=HIGH일 때 PWM LOW 구간에서 모터 동작 → duty=100-50=50%
        R_GPT_DutyCycleSet(&g_timer_gpt3_ctrl,
                           (uint32_t)(PWM_PERIOD_COUNTS * 0.5f), GPT_IO_PIN_GTIOCA);
    } else { // DIR_DOWN
        // Direction LOW: CCW 회전
        R_IOPORT_PinWrite(&g_ioport_ctrl, MOTOR_DIR_PIN, BSP_IO_LEVEL_LOW);
        // Direction=LOW일 때 PWM HIGH 구간에서 모터 동작 → duty=50%
        R_GPT_DutyCycleSet(&g_timer_gpt3_ctrl,
                           (uint32_t)(PWM_PERIOD_COUNTS * 0.5f), GPT_IO_PIN_GTIOCA);
    }
}

/* ───────────────────────────────────────────────
   DC 모터 정지
─────────────────────────────────────────────── */
void dc_motor_stop(void) {
    R_IOPORT_PinWrite(&g_ioport_ctrl, MOTOR_ENABLE_PIN, BSP_IO_LEVEL_LOW);
    R_GPT_DutyCycleSet(&g_timer_gpt3_ctrl, 0, GPT_IO_PIN_GTIOCA);
}

/* ───────────────────────────────────────────────
   서보 모터 각도 설정
   0° → 문 닫힘 (초기 상태)
   180° → 문 열림
─────────────────────────────────────────────── */
void servo_set_angle(uint8_t deg) {
    R_GPT_DutyCycleSet(&g_timer_gpt0_ctrl, servo_duty(deg), GPT_IO_PIN_GTIOCA);
}
