#pragma once

typedef enum {
    RLCD_BTN_NEXT,
    RLCD_BTN_PREV,
} rlcd_btn_t;

typedef void (*rlcd_btn_cb_t)(rlcd_btn_t btn);

// Optional — if the board exposes physical buttons, hook them here.
// Implement after M1 confirms what GPIOs the board has.
void buttons_init(rlcd_btn_cb_t cb);
