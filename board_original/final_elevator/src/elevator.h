/*
 * elevator.h — 엘리베이터 제어 시스템 공용 헤더
 *   상태/방향/층 열거형, 시스템 구조체, CAN·UART·GPIO 상수, 함수 원형 선언
 */
#ifndef ELEVATOR_H
#define ELEVATOR_H

#include "hal_data.h"
#include <stdbool.h>
#include <stdint.h>

// ───────────────────────────────────────────────
// 상태 열거형
// ───────────────────────────────────────────────
typedef enum {
    ELEV_IDLE       = 0, // 대기 상태 (예약 없음)
    ELEV_MOVING,         // 이동 중 (DC모터 ON, 3초/층)
    ELEV_ARRIVED,        // 목적지 도착 (DAC 재생 완료 후 1초 대기)
    ELEV_DOOR_OPEN,      // 문 열림 (Servo 180°, 2초 자동 대기)
    ELEV_DOOR_CLOSE      // 문 닫힘 (Servo 0°, 1초 대기 후 IDLE 복귀)
} ElevatorState_t;

// ───────────────────────────────────────────────
// 방향 열거형
// ───────────────────────────────────────────────
typedef enum {
    DIR_NONE = 0,
    DIR_UP,   // 상승: DC모터 CW 회전
    DIR_DOWN  // 하강: DC모터 CCW 회전
} Direction_t;

// ───────────────────────────────────────────────
// 층 열거형 — IRQ/UART 콜백 가독성 향상용
// ───────────────────────────────────────────────
typedef enum {
    FLOOR_1 = 1,
    FLOOR_2 = 2,
    FLOOR_3 = 3
} Floor_t;

// ───────────────────────────────────────────────
// 엘리베이터 시스템 구조체
// ───────────────────────────────────────────────
typedef struct {
    ElevatorState_t state;
    uint8_t         current_floor;     // 현재층 1~3 (3초마다 타이머 갱신)
    uint8_t         goal_floor;        // 목적지층 1~3
    Direction_t     direction;         // 현재 이동 방향 (도착 후 다음 목적지 우선순위 판단용)
    bool            floor_signal[4];   // 층별 예약 플래그 [1][2][3] (인덱스 0 미사용)
    uint32_t        state_timer_ms;    // 상태 진입 시각 스냅샷 (ms_counter 기준)
    uint32_t        door_open_time_ms; // DOOR_OPEN 진입 시각 (2초 자동 닫힘 기준점)
    bool            door_close_req;    // UART Door Close 즉시 닫힘 요청 플래그
} ElevatorSystem_t;

// 전역 인스턴스 (elevator.c에서 정의)
extern ElevatorSystem_t  elevator;
extern volatile uint32_t ms_counter;        // AGT 1ms 카운터 (hal_entry.c 정의)
extern volatile uint8_t  irq_floor_pending; // ISR→메인루프 전달 래치 (hal_entry.c 정의)

// ───────────────────────────────────────────────
// 시간 상수 — 매직넘버 제거로 가독성/유지보수성 향상
// ───────────────────────────────────────────────
#define FLOOR_TRAVEL_MS  3000U // 층간 이동 시간 (교안 p.5: 3[s]/층)
#define DOOR_OPEN_MS     2000U // 문 열림 유지 시간 (교안 p.6: After 2[s])
#define DOOR_WAIT_MS     1000U // 도착 후/문 닫힘 후 대기 (교안 p.6: After 1[s])

// ───────────────────────────────────────────────
// CAN 메시지 상수 (8byte ASCII)
// ───────────────────────────────────────────────
#define CAN_MSG_OKAY     "CAN OKAY" // 프로그램 시작 시 1회 송신
#define CAN_MSG_MOVETO1  "MOVETO 1" // DC모터 동작 시작/목적지 변경 (목적지 1층)
#define CAN_MSG_MOVETO2  "MOVETO 2" // DC모터 동작 시작/목적지 변경 (목적지 2층)
#define CAN_MSG_MOVETO3  "MOVETO 3" // DC모터 동작 시작/목적지 변경 (목적지 3층)
#define CAN_MSG_ARRIV1   "ARRITO 1" // DC모터 정지(도착) (1층)
#define CAN_MSG_ARRIV2   "ARRITO 2" // DC모터 정지(도착) (2층)
#define CAN_MSG_ARRIV3   "ARRITO 3" // DC모터 정지(도착) (3층)
#define CAN_MSG_DOR_OPEN "DOR OPEN" // 문 열림 시작
#define CAN_MSG_DORCLOSE "DORCLOSE" // 문 닫힘 시작

// ───────────────────────────────────────────────
// UART 제어 비트 마스크 (SCI-UART 1바이트 프로토콜)
// bit[3:2]=FS[1:0](층), bit6=DC[1](문열기), bit5=DC[0](문닫기)
// ───────────────────────────────────────────────
#define UART_FS_MASK    0x0C // bit[3:2] 마스크: 층 선택 필드
#define UART_FS_1F      0x04 // 01 → 1층 선택
#define UART_FS_2F      0x08 // 10 → 2층 선택
#define UART_FS_3F      0x0C // 11 → 3층 선택
#define UART_DOOR_OPEN  0x40 // bit6: 문 열기 (IDLE/DOOR_OPEN 상태에서만 유효)
#define UART_DOOR_CLOSE 0x20 // bit5: 문 닫기 (DOOR_OPEN 상태에서만 유효, 즉시 닫힘)

// ───────────────────────────────────────────────
// GPIO 핀 정의
// ───────────────────────────────────────────────
// LED: 층 예약 표시 (Active LOW) — PA08/PA09/PA10
#define LED1_PIN  BSP_IO_PORT_10_PIN_08 // 1층 예약 LED
#define LED2_PIN  BSP_IO_PORT_10_PIN_09 // 2층 예약 LED
#define LED3_PIN  BSP_IO_PORT_10_PIN_10 // 3층 예약 LED

// DC 모터 H-Bridge 제어 핀 (GPT3 PWM과 조합)
#define MOTOR_ENABLE_PIN  BSP_IO_PORT_09_PIN_00 // Enable: HIGH=구동, LOW=정지
#define MOTOR_DIR_PIN     BSP_IO_PORT_09_PIN_01 // Direction: HIGH=CW(상승), LOW=CCW(하강)

// ───────────────────────────────────────────────
// 함수 선언
// ───────────────────────────────────────────────
void     elevator_state_machine(void);        // 상태 머신 메인 — hal_entry 루프에서 호출
void     elevator_set_signal(uint8_t floor);   // 층 예약 처리 — IRQ/UART 콜백에서 호출

bool     has_pending_signal(void);       // 예약 층 존재 여부 확인
void     set_goal_and_start(void);       // IDLE → 이동 or ARRIVED 진입
void     determine_next_goal(void);      // 도착 후 다음 예약 처리 (방향 우선)
void     update_floor_by_timer(void);    // MOVING 중 3초마다 current_floor 갱신

void     led_on(uint8_t floor);          // 층 예약 시 LED 점등 (Active LOW)
void     led_off(uint8_t floor);         // 문 닫힘 후 LED 소등

void     can_send(uint32_t id, const char *msg); // CAN 송신 — hal_entry.c 구현
void     dac_play_arrival(void);         // DAC 도착음 재생 시작 — hal_entry.c 구현
bool     dac_done(void);                 // DAC 재생 완료 여부 반환

void     reset_state_timer(void);        // 현재 상태 진입 시각 기록
uint32_t elapsed_ms(void);               // 상태 진입 후 경과 시간(ms) 반환

#endif // ELEVATOR_H
