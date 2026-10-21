/*
 * hal_entry.c — 시스템 초기화, 메인 루프, 모든 ISR/콜백 처리
 *   AGT(1ms), IRQ(층 버튼), UART(GUI), CAN(모니터링), GPT-DAC(도착음)
 */
#include "elevator.h"
#include "motor.h"
#include "fnd.h"
#include "hal_data.h"

FSP_CPP_HEADER
void R_BSP_WarmStart(bsp_warm_start_event_t event);
FSP_CPP_FOOTER

// AGT 콜백에서 1ms마다 증가 — 모든 시간 기반 로직의 기준
volatile uint32_t ms_counter = 0;

// IRQ ISR → 메인루프 전달 래치 (0=없음, 1~3=해당층)
volatile uint8_t irq_floor_pending = 0;

// ───────────────────────────────────────────────
// DAC 도착음 자원
// sound1[]: Arrival Sound.c의 unsigned char 배열 (155616 bytes)
// arrival_sound: uint16_t 포인터 재해석 (LE 2바이트 = 1샘플)
// arrival_sound_len: 총 샘플 수 = 155616 / 2 = 77808
// ───────────────────────────────────────────────
extern unsigned char sound1[];
#define arrival_sound     ((const uint16_t *)(sound1))
#define arrival_sound_len (155616U / 2U)

static volatile uint32_t dac_idx      = 0;     // 현재 재생 샘플 위치
static volatile bool     dac_playing  = false; // GPT DAC 타이머 동작 중 여부
static volatile bool     dac_finished = false; // 전체 재생 완료 플래그

// ───────────────────────────────────────────────
// CAN 송신 — Board → PC
// ───────────────────────────────────────────────
void can_send(uint32_t id, const char *msg) {
    can_frame_t frame;
    frame.id               = id;
    frame.id_mode          = CAN_ID_MODE_STANDARD;
    frame.type             = CAN_FRAME_TYPE_DATA;
    frame.data_length_code = 8;
    for (int i = 0; i < 8; i++) {
        // null 이후는 공백으로 채워 8바이트 고정 길이 유지
        frame.data[i] = (uint8_t)(msg[i] ? msg[i] : ' ');
    }
    // 전송 실패 시 재시도 없이 무시
    fsp_err_t err = R_CAN_Write(&g_can0_ctrl, CAN_MAILBOX_ID_0, &frame);
    (void)err;
}

// Board→PC 단방향 송신 전용, 수신 이벤트 미사용
void can_callback(can_callback_args_t *p_args) {
    (void)p_args;
}

// ───────────────────────────────────────────────
// DAC 도착음 재생 시작
// ───────────────────────────────────────────────
void dac_play_arrival(void) {
    dac_idx      = 0;
    dac_finished = false;
    dac_playing  = true;
    R_GPT_Start(&g_timer_gpt_dac_ctrl);
}

bool dac_done(void) {
    return dac_finished; // gpt_dac_callback에서 set
}

// ───────────────────────────────────────────────
// 시스템 초기화 — hal_entry 진입 직후 1회 호출
// ───────────────────────────────────────────────
static void system_init(void) {
    // SW1(IRQ11), SW2(IRQ12), SW3(IRQ13): 1~3층 버튼
    R_ICU_ExternalIrqOpen(&g_external_irq11_ctrl, &g_external_irq11_cfg);
    R_ICU_ExternalIrqEnable(&g_external_irq11_ctrl);
    R_ICU_ExternalIrqOpen(&g_external_irq12_ctrl, &g_external_irq12_cfg);
    R_ICU_ExternalIrqEnable(&g_external_irq12_ctrl);
    R_ICU_ExternalIrqOpen(&g_external_irq13_ctrl, &g_external_irq13_cfg);
    R_ICU_ExternalIrqEnable(&g_external_irq13_ctrl);

    // AGT0: 1ms 주기 타이머 (ms_counter 기준)
    R_AGT_Open(&g_timer_agt0_ctrl, &g_timer_agt0_cfg);
    R_AGT_Start(&g_timer_agt0_ctrl);

    // GPT0(Servo 20ms PWM), GPT3(DC모터 20ms PWM)
    motor_init();

    // SCI UART0: GUI 수신 (115200bps, 8bit, None, 1stop)
    R_SCI_UART_Open(&g_uart0_ctrl, &g_uart0_cfg);

    // CAN0: Board→PC 모니터링 (500kbps, Standard ID)
    R_CAN_Open(&g_can0_ctrl, &g_can0_cfg);

    // DAC0: P014 아날로그 출력 (12bit, 도착음용)
    R_DAC_Open(&g_dac0_ctrl, &g_dac0_cfg);
    R_DAC_Start(&g_dac0_ctrl);

    // FND: 초기 표시 현재층=1, 목표층=1
    fnd_init();

    // GPT(DAC 샘플레이트 타이머, 48kHz): Open만, Start는 dac_play_arrival()에서
    R_GPT_Open(&g_timer_gpt_dac_ctrl, &g_timer_gpt_dac_cfg);
}

// ───────────────────────────────────────────────
// 메인 루프 — 비동기 상태 머신 + FND 멀티플렉싱
// ───────────────────────────────────────────────
void hal_entry(void) {
    system_init();
    can_send(0x0, CAN_MSG_OKAY); // 프로그램 시작 알림

    while (1) {
        // ISR에서 래치된 층 신호 소비 (ISR 최소화 설계)
        if (irq_floor_pending) {
            elevator_set_signal(irq_floor_pending);
            irq_floor_pending = 0;
        }
        elevator_state_machine(); // 상태 머신
        fnd_update();             // FND 4자리 멀티플렉싱 (약 8ms)
    }
}

// ───────────────────────────────────────────────
// IRQ 콜백 — SW1/SW2/SW3 층 예약 (ISR 최소화)
// ───────────────────────────────────────────────
void R_IRQ_Interrupt(external_irq_callback_args_t *p_args) {
    switch (p_args->channel) {
        case 11: irq_floor_pending = FLOOR_1; break; // SW1 → 1층
        case 12: irq_floor_pending = FLOOR_2; break; // SW2 → 2층
        case 13: irq_floor_pending = FLOOR_3; break; // SW3 → 3층
        default: break;
    }
}

// ───────────────────────────────────────────────
// AGT 콜백 — 1ms마다 ms_counter 증가
// ───────────────────────────────────────────────
void agt_callback(timer_callback_args_t *p_args) {
    if (p_args->event == TIMER_EVENT_CYCLE_END) {
        ms_counter++;
    }
}

// ───────────────────────────────────────────────
// UART 콜백 — GUI 1바이트 명령 처리
// bit[3:2]=층, bit6=문열기, bit5=문닫기
// ───────────────────────────────────────────────
void uart_callback(uart_callback_args_t *p_args) {
    // 수신 에러 발생 시 해당 바이트 폐기
    if (p_args->event == UART_EVENT_ERR_OVERFLOW ||
        p_args->event == UART_EVENT_ERR_FRAMING  ||
        p_args->event == UART_EVENT_ERR_PARITY) {
        return;
    }
    if (p_args->event != UART_EVENT_RX_CHAR) return;

    uint8_t data = (uint8_t)p_args->data;

    // bit[3:2] 층 선택 디코딩 → IRQ와 동일하게 elevator_set_signal 처리
    switch (data & UART_FS_MASK) {
        case UART_FS_1F: elevator_set_signal(FLOOR_1); break; // 01 → 1층
        case UART_FS_2F: elevator_set_signal(FLOOR_2); break; // 10 → 2층
        case UART_FS_3F: elevator_set_signal(FLOOR_3); break; // 11 → 3층
        default: break; // 층 정보 없는 패킷 (Door 명령 전용)
    }

    // bit6 Door Open — QnA2: 열림 명령마다 CAN 송신
    // 유효 상태: IDLE(현재층 문 강제 개방), DOOR_OPEN(2초 타이머 리셋)
    if (data & UART_DOOR_OPEN) {
        if (elevator.state == ELEV_IDLE) {
            elevator.goal_floor        = elevator.current_floor;
            elevator.state             = ELEV_DOOR_OPEN;
            elevator.door_open_time_ms = ms_counter;
            elevator.door_close_req    = false;
            servo_set_angle(180);
            can_send(0x3, CAN_MSG_DOR_OPEN);
        } else if (elevator.state == ELEV_DOOR_OPEN) {
            elevator.door_open_time_ms = ms_counter; // 2초 자동 닫힘 타이머 리셋
            elevator.door_close_req    = false;
            can_send(0x3, CAN_MSG_DOR_OPEN);         // QnA2: 재요청도 송신
        }
    }

    // bit5 Door Close — DOOR_OPEN 상태에서만 유효 (즉시 닫힘 요청)
    if (data & UART_DOOR_CLOSE) {
        if (elevator.state == ELEV_DOOR_OPEN) {
            elevator.door_close_req = true;
        }
    }
}

// ───────────────────────────────────────────────
// GPT DAC 콜백 — 48kHz마다 호출, 샘플 순차 출력
// ───────────────────────────────────────────────
void gpt_dac_callback(timer_callback_args_t *p_args) {
    if (!dac_playing) return; // 재생 요청 없으면 즉시 탈출
    if (p_args->event != TIMER_EVENT_CYCLE_END) return;

    if (dac_idx < arrival_sound_len) {
        // 48kHz 주기마다 다음 PCM 샘플을 12bit DAC에 출력
        R_DAC_Write(&g_dac0_ctrl, arrival_sound[dac_idx++]);
    } else {
        // 재생 완료: 타이머 정지 + 플래그 set으로 ARRIVED에 "문 열기 가능" 통보
        dac_playing  = false;
        dac_finished = true;
        R_GPT_Stop(&g_timer_gpt_dac_ctrl);
    }
}

// ───────────────────────────────────────────────
// BSP WarmStart — POST_C 이벤트에서 IOPORT 초기화
// ───────────────────────────────────────────────
void R_BSP_WarmStart(bsp_warm_start_event_t event) {
    if (BSP_WARM_START_POST_C == event) {
        R_IOPORT_Open(&g_ioport_ctrl, g_ioport.p_cfg);
    }
}
