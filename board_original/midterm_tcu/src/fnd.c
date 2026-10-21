#include "hal_data.h"
#include "fnd.h"
#include "tcu.h"

static const bsp_io_port_pin_t digit_pin[4]={
    BSP_IO_PORT_03_PIN_05,BSP_IO_PORT_03_PIN_06,
    BSP_IO_PORT_03_PIN_07,BSP_IO_PORT_03_PIN_08};
static const bsp_io_port_pin_t seg_pin[8]={
    BSP_IO_PORT_06_PIN_14,BSP_IO_PORT_06_PIN_04,
    BSP_IO_PORT_06_PIN_05,BSP_IO_PORT_06_PIN_06,
    BSP_IO_PORT_06_PIN_07,BSP_IO_PORT_06_PIN_11,
    BSP_IO_PORT_06_PIN_12,BSP_IO_PORT_06_PIN_13};

void fnd_reset(){
    for(int i=0;i<4;i++) R_IOPORT_PinWrite(&g_ioport_ctrl,digit_pin[i],BSP_IO_LEVEL_LOW);
    for(int i=0;i<8;i++) R_IOPORT_PinWrite(&g_ioport_ctrl,seg_pin[i],BSP_IO_LEVEL_HIGH);
}

void fnd_show(uint8_t d, uint8_t data){
    fnd_reset(); // 켜기 전에 리셋

    R_IOPORT_PinWrite(&g_ioport_ctrl, digit_pin[d], BSP_IO_LEVEL_HIGH);
    for(int i=0; i<8; i++){
        uint8_t b = (data >> (7 - i)) & 1;
        R_IOPORT_PinWrite(&g_ioport_ctrl, seg_pin[i], (bsp_io_level_t)b);
    }

    // 모든 자릿수를 2ms 동안만 켜두기
    R_BSP_SoftwareDelay(2, BSP_DELAY_UNITS_MILLISECONDS);

    fnd_reset();
}

void fnd_update(tcu_state_t *s) {
    const uint8_t num[] = {0x81,0xCF, 0x92, 0x86, 0xCC}; // 0~4
    const uint8_t lever[] = {0x98, 0xEA, 0xC2, 0xFA};    // P N D R

    uint8_t gear_seg;
    if(s->lever == LEVER_P){
        gear_seg = 0xFE;                          // '-'
    }
    else if(s->gear >= 1 && s->gear <= 4){
        gear_seg = num[s->gear];                  // 1~4
    }
    else{
        gear_seg = 0xFF;                          // 공백
    }
    uint8_t buf[4] = {
        lever[s->lever],
        (s->mode == MODE_AUTO) ? 0x88 : 0xE0,    // A or b
        gear_seg,
        s->engine_err ? 0x86 : 0xFF              // E or 공백
                };

    for(int i = 0; i < 4; i++) {
        fnd_show((uint8_t)i, buf[i]);
    }
}
