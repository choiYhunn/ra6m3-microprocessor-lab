#ifndef TCU_H
#define TCU_H
#include "hal_data.h"
typedef enum { LEVER_P=0, LEVER_N, LEVER_D, LEVER_R } lever_state_t;
typedef enum { MODE_AUTO=0, MODE_MANUAL } drive_mode_t;
// 현재 상태 저장 구조체
typedef struct {
    lever_state_t lever;
    drive_mode_t  mode;
    uint8_t       gear;       // 1~4
    bool          engine_err; // CDS >= 800
} tcu_state_t;
void tcu_handle_sw1(tcu_state_t *s);
void tcu_handle_sw2(tcu_state_t *s);
void tcu_handle_sw3(tcu_state_t *s, float tps);
void tcu_handle_sw4(tcu_state_t *s, float tps);
void tcu_auto_shift(tcu_state_t *s, float tps);
#endif
