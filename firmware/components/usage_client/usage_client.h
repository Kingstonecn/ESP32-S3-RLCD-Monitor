#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    int64_t  tokens_used;
    double   cost_usd;
    int32_t  minutes_remaining;
    int32_t  percent_used_x100;  // -1 if bridge can't compute
    bool     valid;
} usage_active_block_t;

typedef struct {
    int64_t tokens_used;
    double  cost_usd;
    int32_t percent_used_x100;   // -1 if bridge can't compute
} usage_bucket_t;

typedef struct {
    char     model[40];
    int64_t  tokens;
    double   cost_usd;
} usage_model_t;

#define USAGE_MAX_MODELS 5

typedef struct {
    int32_t util_5h_x100;   // 0..100, -1 unknown
    int32_t util_7d_x100;   // 0..100, -1 unknown
    int32_t reset_5h_min;   // minutes until 5h reset, -1 unknown
    char    status[12];     // "ok"/"stale"/...
} usage_limits_t;

typedef struct {
    double  temp_c;
    double  temp_min;
    double  temp_max;
    int32_t code;
    char    condition[16];
    char    icon[10];       // clear/partly/cloud/rain/snow/fog
    char    city[16];
    bool    valid;
} usage_weather_t;

typedef struct {
    double  balance;
    char    currency[8];
    double  granted;
    double  topped;
    int64_t today_tokens;
    double  today_cost_cny;
    float   today_cache_pct;    // 0-100, -1 unknown
    char    today_label[12];    // "TODAY" or "YESTERDAY"
    int64_t month_tokens;
    double  month_cost_cny;
    float   month_cache_pct;    // 0-100, -1 unknown
    bool    valid;
} usage_deepseek_t;

typedef struct {
    int32_t rolling_pct;       // 0-100
    int32_t rolling_reset_min; // minutes until reset
    char    rolling_status[16];// "ok" | "rate-limited"
    int32_t weekly_pct;
    int32_t weekly_reset_min;
    char    weekly_status[16];
    int32_t monthly_pct;
    int32_t monthly_reset_min;
    char    monthly_status[16];
    bool    valid;
} usage_opencode_t;

typedef struct {
    char                  updated_at[32];      // RFC3339
    usage_active_block_t  active_block;
    usage_bucket_t        weekly;
    usage_bucket_t        today;
    usage_bucket_t        month;
    usage_bucket_t        lifetime;
    usage_model_t         models[USAGE_MAX_MODELS];
    int                   model_count;
    usage_limits_t        limits;
    usage_weather_t       weather;
    usage_deepseek_t      deepseek;
    usage_opencode_t      opencode;
    bool                  stale;
} usage_report_t;

// `token` may be NULL or "" for no auth; otherwise sent as X-RLCD-Token.
esp_err_t usage_client_fetch(const char *url, const char *token, usage_report_t *out);

#ifdef __cplusplus
}
#endif
