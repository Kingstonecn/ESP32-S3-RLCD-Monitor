#pragma once

#include "net/usage_client.h"

void ui_main_init(void);
void ui_main_update(const usage_report_t *r);
void ui_main_mark_stale(void);
