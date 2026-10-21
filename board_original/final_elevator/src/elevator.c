/*
 * elevator.c — 엘리베이터 상태 머신 및 SCAN 스케줄링 로직
 *   5상태 비동기 상태 머신(IDLE/MOVING/ARRIVED/DOOR_OPEN/DOOR_CLOSE)
 */
#include "elevator.h"
#include "motor.h"
#include "fnd.h"

// 전역 인스턴스 — 초기 상태: 1층 대기
ElevatorSystem_t elevator = {
    .state          = ELEV_IDLE,
    .current_floor  = 1,
    .goal_floor     = 1,
    .direction      = DIR_NONE,
    .floor_signal   = {false, false, false, false},
    .door_close_req = false,
};

// 층간 이동 시간 측정용 타임스탬프
// start_moving_to()에서 초기화, update_floor_by_timer()에서 3초마다 갱신
static uint32_t last_floor_update_ms = 0;

// ───────────────────────────────────────────────
// 타이머 헬퍼
// ───────────────────────────────────────────────
void reset_state_timer(void) {
    elevator.state_timer_ms = ms_counter; // 현재 상태 진입 시각 기록
}

uint32_t elapsed_ms(void) {
    return ms_counter - elevator.state_timer_ms; // 상태 진입 후 경과 시간
}

// ───────────────────────────────────────────────
// LED 제어 (Active LOW — LOW=점등, HIGH=소등)
// ───────────────────────────────────────────────
void led_on(uint8_t floor) {
    const bsp_io_port_pin_t pins[4] = {0, LED1_PIN, LED2_PIN, LED3_PIN};
    if (floor < 1 || floor > 3) return; // 유효 범위 밖이면 무시 (방어 코드)
    R_IOPORT_PinWrite(&g_ioport_ctrl, pins[floor], BSP_IO_LEVEL_LOW);
}

void led_off(uint8_t floor) {
    const bsp_io_port_pin_t pins[4] = {0, LED1_PIN, LED2_PIN, LED3_PIN};
    if (floor < 1 || floor > 3) return;
    R_IOPORT_PinWrite(&g_ioport_ctrl, pins[floor], BSP_IO_LEVEL_HIGH);
}

// ───────────────────────────────────────────────
// 시그널 처리 — IRQ/UART 콜백에서 호출
// 1) floor_signal 설정 + LED ON (LED 켜진 동안 같은 층 재입력은 무시)
// 2) 상태별 목적지 갱신:
//    IDLE   → set_goal_and_start()로 즉시 출발 판단
//    MOVING → 진행 방향상 경유 가능 층이면 goal 단축 + CAN MOVETO 재전송
// ───────────────────────────────────────────────
void elevator_set_signal(uint8_t floor) {
    if (floor < 1 || floor > 3) return;
    if (elevator.floor_signal[floor]) return; // 이미 예약된(LED 켜진) 층은 재입력 무시

    elevator.floor_signal[floor] = true;
    led_on(floor);

    if (elevator.state == ELEV_IDLE) {
        set_goal_and_start();
    } else if (elevator.state == ELEV_MOVING) {
        // UP 방향: 현재층 위 & 기존 목표층 아래(=경유 가능)이면 목적지 단축
        if (elevator.direction == DIR_UP
                && floor > elevator.current_floor
                && floor < elevator.goal_floor) {
            elevator.goal_floor = floor;
            fnd_set_goal(floor);
            // 이동 중 목적지 변경 시 CAN MOVETO 즉시 재전송
            can_send(0x1, (floor == FLOOR_1) ? CAN_MSG_MOVETO1 :
                          (floor == FLOOR_2) ? CAN_MSG_MOVETO2 : CAN_MSG_MOVETO3);
        }
        // DOWN 방향: 현재층 미만 & 현재 목표층 초과(=경유 가능)이면 목적지 단축
        else if (elevator.direction == DIR_DOWN
                && floor < elevator.current_floor
                && floor > elevator.goal_floor) {
            elevator.goal_floor = floor;
            fnd_set_goal(floor);
            can_send(0x1, (floor == FLOOR_1) ? CAN_MSG_MOVETO1 :
                          (floor == FLOOR_2) ? CAN_MSG_MOVETO2 : CAN_MSG_MOVETO3);
        }
        // 그 외(이미 지나친 층, 반대 방향 층)는 floor_signal만 유지 →
        // determine_next_goal()에서 도착 후 처리
    }
}

// ───────────────────────────────────────────────
// 예약 층 존재 여부 확인 (인덱스 1~3)
// ───────────────────────────────────────────────
bool has_pending_signal(void) {
    for (uint8_t i = 1; i <= 3; i++) {
        if (elevator.floor_signal[i]) return true;
    }
    return false;
}

// ───────────────────────────────────────────────
// 내부 헬퍼 — 현재층 도착 시퀀스 진입 (모터 동작 없음)
// 동일층은 DC모터가 안 도므로 MOVETO/ARRITO 모두 미전송, 도착음만 재생
// set_goal_and_start / determine_next_goal에서 공용 사용
// ───────────────────────────────────────────────
static void enter_same_floor_arrival(void) {
    elevator.goal_floor = elevator.current_floor;
    elevator.state      = ELEV_ARRIVED;
    elevator.direction  = DIR_NONE;
    fnd_set_goal(elevator.current_floor); // Digit3 동기화 (FND 실시간성)
    dac_play_arrival();                   // 도착음 재생 시작
    reset_state_timer();
}

// ───────────────────────────────────────────────
// 내부 헬퍼 — 목표층으로 이동 시작
// ───────────────────────────────────────────────
static void start_moving_to(uint8_t floor) {
    elevator.goal_floor = floor;
    elevator.direction  = (floor > elevator.current_floor) ? DIR_UP : DIR_DOWN;
    elevator.state      = ELEV_MOVING;
    dc_motor_start(elevator.direction); // DC모터 구동 (50% duty)
    fnd_motor_running(true);            // FND Digit1/4 회전 애니메이션 ON
    // CAN MOVETO N: DC모터 동작 시작 시점 송신
    can_send(0x1, (floor == FLOOR_1) ? CAN_MSG_MOVETO1 :
                   (floor == FLOOR_2) ? CAN_MSG_MOVETO2 : CAN_MSG_MOVETO3);
    fnd_set_goal(floor);
    reset_state_timer();
    last_floor_update_ms = ms_counter; // 층간 3초 타이머 기준점 초기화
}

// ───────────────────────────────────────────────
// IDLE → 이동 or 도착 시퀀스 진입
// - 현재층 예약: 모터 없이 바로 ARRIVED (도착음 + 문 열기), CAN 미전송
// - 타층 예약  : nearest-first로 가장 가까운 층 선택 후 start_moving_to
// ───────────────────────────────────────────────
void set_goal_and_start(void) {
    // 현재층이 예약된 경우: 동일층 도착 처리
    if (elevator.floor_signal[elevator.current_floor]) {
        enter_same_floor_arrival();
        return;
    }

    // 가장 가까운 예약 층 선택 (nearest-first)
    uint8_t best     = 0;
    uint8_t min_dist = 255;
    for (uint8_t i = 1; i <= 3; i++) {
        if (!elevator.floor_signal[i]) continue;
        uint8_t d = (i > elevator.current_floor)
                    ? (i - elevator.current_floor)
                    : (elevator.current_floor - i);
        if (d < min_dist) { min_dist = d; best = i; }
    }
    if (best == 0) return; // 예약 없음 (방어 코드)

    start_moving_to(best);
}

// ───────────────────────────────────────────────
// MOVING 중 층간 이동 시간 추적 — 3초마다 current_floor 갱신
// 층간 이동 3초/층 기준
// ───────────────────────────────────────────────
void update_floor_by_timer(void) {
    // 3000ms 미경과 시 early return (층 1개 이동에 3초 소요)
    if (ms_counter - last_floor_update_ms < FLOOR_TRAVEL_MS) return;
    last_floor_update_ms = ms_counter;

    // DIR_UP: 3층 상한 초과 방지하며 가상 층 +1
    if (elevator.direction == DIR_UP && elevator.current_floor < 3)
        elevator.current_floor++;
    // DIR_DOWN: 1층 하한 미만 방지하며 가상 층 -1
    else if (elevator.direction == DIR_DOWN && elevator.current_floor > 1)
        elevator.current_floor--;

    fnd_set_current(elevator.current_floor); // FND Digit2 실시간 갱신
}

// ───────────────────────────────────────────────
// 도착 후 다음 예약 층 결정 — 기존 방향 우선
// DIR_UP  : 현재층 위 우선 → 없으면 아래
// DIR_DOWN: 현재층 아래 우선 → 없으면 위
// DIR_NONE: nearest-first
// ───────────────────────────────────────────────
void determine_next_goal(void) {
    if (!has_pending_signal()) return;

    // 현재층이 예약돼 있으면 모터 없이 동일층 도착 처리 (MOVETO/ARRITO 미전송)
    // 문 열림 중 현재층 입력 등으로 거리 0 목적지가 잡혀 0초 MOVETO가 튀는 것 방지
    if (elevator.floor_signal[elevator.current_floor]) {
        enter_same_floor_arrival();
        return;
    }

    uint8_t next = 0;

    if (elevator.direction == DIR_UP) {
        // 기존 UP 방향 우선: 현재층 위쪽부터 탐색
        for (uint8_t i = elevator.current_floor + 1; i <= 3 && !next; i++)
            if (elevator.floor_signal[i]) next = i;
        // UP에 없으면 아래층 탐색 (방향 전환)
        for (int8_t i = (int8_t)elevator.current_floor - 1; i >= 1 && !next; i--)
            if (elevator.floor_signal[i]) next = (uint8_t)i;

    } else if (elevator.direction == DIR_DOWN) {
        // 기존 DOWN 방향 우선: 현재층 아래쪽부터 탐색
        for (int8_t i = (int8_t)elevator.current_floor - 1; i >= 1 && !next; i--)
            if (elevator.floor_signal[i]) next = (uint8_t)i;
        // DOWN에 없으면 위층 탐색 (방향 전환)
        for (uint8_t i = elevator.current_floor + 1; i <= 3 && !next; i++)
            if (elevator.floor_signal[i]) next = i;

    } else {
        // DIR_NONE: nearest-first
        uint8_t min_dist = 255;
        for (uint8_t i = 1; i <= 3; i++) {
            if (!elevator.floor_signal[i]) continue;
            uint8_t d = (i > elevator.current_floor)
                        ? (i - elevator.current_floor)
                        : (elevator.current_floor - i);
            if (d < min_dist) { min_dist = d; next = i; }
        }
    }

    if (next) start_moving_to(next);
}

// ───────────────────────────────────────────────
// 엘리베이터 상태 머신 — hal_entry 메인 루프에서 매 사이클 호출
// 시퀀스: IDLE → MOVING → ARRIVED → DOOR_OPEN → DOOR_CLOSE → IDLE
// ───────────────────────────────────────────────
void elevator_state_machine(void) {
    switch (elevator.state) {

        case ELEV_IDLE:
            if (has_pending_signal()) set_goal_and_start();
            break;

        case ELEV_MOVING:
            update_floor_by_timer();
            // 목적지 도착 판정
            if (elevator.current_floor == elevator.goal_floor) {
                elevator.state = ELEV_ARRIVED;
                dc_motor_stop();
                fnd_motor_running(false);
                dac_play_arrival();
                // CAN ARRITO N: 실제 정지한 목적지에서만 송신
                can_send(0x2, (elevator.goal_floor == FLOOR_1) ? CAN_MSG_ARRIV1 :
                               (elevator.goal_floor == FLOOR_2) ? CAN_MSG_ARRIV2 : CAN_MSG_ARRIV3);
                reset_state_timer();
            }
            break;

        case ELEV_ARRIVED:
            // 도착음 완료 && 최소 1초 경과 → 문 열기
            if (dac_done() && elapsed_ms() >= DOOR_WAIT_MS) {
                elevator.state = ELEV_DOOR_OPEN;
                servo_set_angle(180);              // 서보 180° → 문 열림
                can_send(0x3, CAN_MSG_DOR_OPEN);
                elevator.door_open_time_ms = ms_counter; // 2초 자동 닫힘 기준점
            }
            break;

        case ELEV_DOOR_OPEN:
            // 2초 경과(자동) 또는 UART Door Close 요청(즉시) 시 문 닫기
            if ((ms_counter - elevator.door_open_time_ms >= DOOR_OPEN_MS)
                    || elevator.door_close_req) {
                elevator.door_close_req = false;
                elevator.state = ELEV_DOOR_CLOSE;
                servo_set_angle(0);                // 서보 0° → 문 닫힘
                can_send(0x4, CAN_MSG_DORCLOSE);
                // 영상 방식: 문 닫힘 시점에 예약 해제 + LED 소등
                elevator.floor_signal[elevator.goal_floor] = false;
                led_off(elevator.goal_floor);
                reset_state_timer();
            }
            break;

        case ELEV_DOOR_CLOSE:
            // 1초 대기 후 IDLE 복귀 (direction 유지 → 방향 우선 로직 활용)
            if (elapsed_ms() >= DOOR_WAIT_MS) {
                elevator.state = ELEV_IDLE;
                if (has_pending_signal()) {
                    determine_next_goal();         // 남은 예약 처리 (방향 우선)
                } else {
                    elevator.direction = DIR_NONE; // 모든 예약 완료 → 방향 초기화
                }
            }
            break;
    }
}
