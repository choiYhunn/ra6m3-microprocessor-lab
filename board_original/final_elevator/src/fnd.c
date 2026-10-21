#include "fnd.h"
#include "elevator.h"

// ───────────────────────────────────────────────
// 핀 정의
// digit_pin[0~3]: Digit1~4 공통 양극(Common Anode) 선택핀
// seg_pin[0~7]  : 세그먼트 제어핀 (Active LOW)
// ───────────────────────────────────────────────
static const bsp_io_port_pin_t digit_pin[4] = {
    BSP_IO_PORT_03_PIN_05, BSP_IO_PORT_03_PIN_06,
    BSP_IO_PORT_03_PIN_07, BSP_IO_PORT_03_PIN_08
};
static const bsp_io_port_pin_t seg_pin[8] = {
    BSP_IO_PORT_06_PIN_14, BSP_IO_PORT_06_PIN_04,
    BSP_IO_PORT_06_PIN_05, BSP_IO_PORT_06_PIN_06,
    BSP_IO_PORT_06_PIN_07, BSP_IO_PORT_06_PIN_11,
    BSP_IO_PORT_06_PIN_12, BSP_IO_PORT_06_PIN_13
};

// ───────────────────────────────────────────────
// 내부 상태 변수
// ───────────────────────────────────────────────
static uint8_t  disp_current = 1;     // Digit2 표시값: 현재층 (1~3)
static uint8_t  disp_goal    = 1;     // Digit3 표시값: 목적지층 (1~3)
static bool     motor_anim   = false; // DC모터 동작 여부 (Digit1/4 애니메이션 ON/OFF)
static uint8_t  anim_idx     = 0;     // 애니메이션 인덱스 (0~5, 200ms마다 갱신)
static uint32_t last_anim_ms = 0;     // 마지막 애니메이션 갱신 시각 (ms_counter 기준)

// ───────────────────────────────────────────────
// 숫자 세그먼트 패턴
// 비트 순서: dp(7) a(6) b(5) c(4) d(3) e(2) f(1) g(0)
// Active LOW: 해당 비트가 0이면 세그먼트 ON
// 0xCF=1, 0x92=2, 0x86=3 (인덱스 0은 미사용 패딩)
// ───────────────────────────────────────────────
static const uint8_t num_seg[4] = { 0x81, 0xCF, 0x92, 0x86 };

// ───────────────────────────────────────────────
// Digit1/4: DC모터 동작 중 회전 애니메이션 (6단계, 200ms/step)
// 비트 순서: dp(7) a(6) b(5) c(4) d(3) e(2) f(1) g(0), Active LOW
// 바깥 테두리(CCW) 순서로 점등: a→f→e→d→c→b
//   [0] 0xBF: a(상단)       bit6=0
//   [1] 0xFD: f(좌측 상단)  bit1=0
//   [2] 0xFB: e(좌측 하단)  bit2=0
//   [3] 0xF7: d(하단)       bit3=0
//   [4] 0xEF: c(우측 하단)  bit4=0
//   [5] 0xDF: b(우측 상단)  bit5=0
// Digit4는 +3 오프셋(6단계의 절반=180° 위상차)으로 반대쪽 세그먼트 표시
// ───────────────────────────────────────────────
static const uint8_t anim_seg[6] = {
    0xBF, // a: 상단
    0xFD, // f: 좌측 상단
    0xFB, // e: 좌측 하단
    0xF7, // d: 하단
    0xEF, // c: 우측 하단
    0xDF  // b: 우측 상단
};

// fnd_reset: 모든 digit OFF, 모든 segment HIGH(소등)
static void fnd_reset(void) {
    for (int i = 0; i < 4; i++)
        R_IOPORT_PinWrite(&g_ioport_ctrl, digit_pin[i], BSP_IO_LEVEL_LOW);
    for (int i = 0; i < 8; i++)
        R_IOPORT_PinWrite(&g_ioport_ctrl, seg_pin[i], BSP_IO_LEVEL_HIGH);
}

// fnd_show: d번 자릿수를 data 패턴으로 2ms 점등 후 소등
static void fnd_show(uint8_t d, uint8_t data) {
    fnd_reset();
    R_IOPORT_PinWrite(&g_ioport_ctrl, digit_pin[d], BSP_IO_LEVEL_HIGH); // 해당 digit 선택
    for (int i = 0; i < 8; i++) {
        uint8_t b = (data >> (7 - i)) & 1; // MSB(dp)부터 LSB(g) 순으로 출력
        R_IOPORT_PinWrite(&g_ioport_ctrl, seg_pin[i], (bsp_io_level_t)b);
    }
    // 멀티플렉싱 2ms 점등 — 잔상 방지용. 시스템 정지 영향 최소화 (교안 허용 구간)
    R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_MILLISECONDS);
    fnd_reset();
}

// 초기화: 현재층 1, 목표층 1, 애니메이션 OFF
void fnd_init(void) {
    disp_current = 1;
    disp_goal    = 1;
    motor_anim   = false;
    anim_idx     = 0;
}

// Digit2 현재층 갱신 (1~3 범위 검사 포함)
void fnd_set_current(uint8_t floor) {
    if (floor >= 1 && floor <= 3) disp_current = floor;
}

// Digit3 목적지층 갱신 (1~3 범위 검사 포함)
void fnd_set_goal(uint8_t floor) {
    if (floor >= 1 && floor <= 3) disp_goal = floor;
}

// DC모터 동작 여부 설정 — true: 애니메이션 ON, false: 정지 후 OFF
void fnd_motor_running(bool on) {
    motor_anim = on;
    if (!on) anim_idx = 0; // 정지 시 인덱스 초기화
}

// ───────────────────────────────────────────────
// fnd_update: 메인 루프에서 매 사이클 호출 (4자리 멀티플렉싱)
// Digit1: 모터 동작 중 → 회전 애니메이션 / 정지 시 → '-' (0xFE)
// Digit2: 현재층 숫자 (1~3)
// Digit3: 목적지층 숫자 (1~3)
// Digit4: 모터 동작 중 → 위상차(+3) 애니메이션 / 정지 시 → '-' (0xFE)
// 0xFE = 1111 1110: g(bit0)=0만 점등 → 가운데 '-' (모터 정지 표시)
// ───────────────────────────────────────────────
void fnd_update(void) {
    // 200ms마다 애니메이션 인덱스 갱신 — 회전 모션 (교안 대비 기준)
    if (motor_anim && (ms_counter - last_anim_ms >= 200)) {
        anim_idx = (anim_idx + 1) % 6;
        last_anim_ms = ms_counter;
    }

    uint8_t d1 = motor_anim ? anim_seg[anim_idx]         : 0xFE; // Digit1
    uint8_t d2 = num_seg[disp_current];                          // Digit2: 현재층
    uint8_t d3 = num_seg[disp_goal];                             // Digit3: 목적지층
    uint8_t d4 = motor_anim ? anim_seg[(anim_idx+3)%6]   : 0xFE; // Digit4

    fnd_show(0, d1);
    fnd_show(1, d2);
    fnd_show(2, d3);
    fnd_show(3, d4);
}
