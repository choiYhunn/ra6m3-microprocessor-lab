#include "tcu.h"
// 전체 상태 관리

// SW1: 레버 P→N→D→R 순환
void tcu_handle_sw1(tcu_state_t *s) {
    s->lever = (lever_state_t)((s->lever + 1) % 4);
    // 레버 변경 시 기어 1단으로 리셋
    if(s->lever == LEVER_D || s->lever == LEVER_N) s->gear = 1;
}

// SW2: AUTO/MANUAL 토글
void tcu_handle_sw2(tcu_state_t *s) {
    s->mode = (s->mode == MODE_AUTO) ? MODE_MANUAL : MODE_AUTO;
}

// SW3: 기어 업 (수동 모드, 조건 만족 시)
void tcu_handle_sw3(tcu_state_t *s, float tps) {
    if(s->mode != MODE_MANUAL) return;
    if(s->lever == LEVER_P) return;
    const float upshift_tps[] = {20, 50, 80, 100}; // 기어1→2, 2→3, 3→4
    if(s->gear < 4 && tps > upshift_tps[s->gear - 1])
        s->gear++;
}

// SW4: 기어 다운 (수동 모드, 조건 만족 시)
void tcu_handle_sw4(tcu_state_t *s, float tps) {
    if(s->mode != MODE_MANUAL) return;
    if(s->lever == LEVER_P) return;
    const float downshift_tps[] = {0, 20, 50, 80}; // 기어2→1, 3→2, 4→3
    if(s->gear > 1 && tps < downshift_tps[s->gear - 1])
        s->gear--;
}

// 자동 변속 (히스테리시스)
void tcu_auto_shift(tcu_state_t *s, float tps) {
    if(s->lever == LEVER_P) return;
    // 히스테리시스: 업시프트/다운시프트 임계값을 +-1% 차이로 설정해 경계값 근처에서 잦은 변속 방지
    switch(s->gear) {
        case 1:
            if(tps > 21) s->gear = 2;
            break;
        case 2:
            if(tps > 51) s->gear = 3;
            else if(tps < 19) s->gear = 1;
            break;
        case 3:
            if(tps > 81) s->gear = 4;
            else if(tps < 49) s->gear = 2;
            break;
        case 4:
            if(tps < 79) s->gear = 3;
            break;
    }
}
