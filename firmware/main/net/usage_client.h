#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    int64_t  tokens_used;
    double   cost_usd;
    int32_t  minutes_remaining;
    int32_t  percent_used_x100;   // -1 = unknown, else 0..10000
    bool     valid;
} usage_active_block_t;

typedef struct {
    int64_t tokens_used;
    double  cost_usd;
    int32_t percent_used_x100;    // -1 = unknown
} usage_bucket_t;

typedef struct {
    char model[40];
    int64_t tokens;
    double  cost_usd;
} usage_model_t;

#define USAGE_MAX_MODELS 5

typedef struct {
    char updated_at[32];          // RFC3339
    usage_active_block_t active_block;
    usage_bucket_t weekly;
    usage_bucket_t today;
    usage_bucket_t month;
    usage_bucket_t lifetime;
    usage_model_t  models[USAGE_MAX_MODELS];
    int            model_count;
    bool           stale;
} usage_report_t;

// `token` may be NULL or "" for no auth; otherwise it is sent as X-RLCD-Token.
esp_err_t usage_client_fetch(const char *url, const char *token, usage_report_t *out);
