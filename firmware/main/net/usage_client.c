#include "usage_client.h"

#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "usage_client";

#define MAX_RESP_BYTES (16 * 1024)

typedef struct {
    char *buf;
    int   len;
    int   cap;
} resp_t;

static esp_err_t http_evt(esp_http_client_event_t *ev)
{
    if (ev->event_id == HTTP_EVENT_ON_DATA && !esp_http_client_is_chunked_response(ev->client)) {
        resp_t *r = (resp_t *) ev->user_data;
        if (r->len + ev->data_len < r->cap) {
            memcpy(r->buf + r->len, ev->data, ev->data_len);
            r->len += ev->data_len;
            r->buf[r->len] = 0;
        }
    }
    return ESP_OK;
}

static int32_t pct_x100(cJSON *v)
{
    if (cJSON_IsNumber(v)) {
        return (int32_t)(v->valuedouble * 10000.0);
    }
    return -1;
}

static void parse_bucket(cJSON *o, usage_bucket_t *out)
{
    cJSON *tok = cJSON_GetObjectItem(o, "tokens_used");
    cJSON *cst = cJSON_GetObjectItem(o, "cost_usd");
    cJSON *pct = cJSON_GetObjectItem(o, "percent_used");
    out->tokens_used = cJSON_IsNumber(tok) ? (int64_t) tok->valuedouble : 0;
    out->cost_usd    = cJSON_IsNumber(cst) ? cst->valuedouble : 0.0;
    out->percent_used_x100 = pct_x100(pct);
}

esp_err_t usage_client_fetch(const char *url, const char *token, usage_report_t *out)
{
    memset(out, 0, sizeof(*out));
    out->active_block.percent_used_x100 = -1;

    char *buf = malloc(MAX_RESP_BYTES);
    if (!buf) return ESP_ERR_NO_MEM;
    resp_t r = { .buf = buf, .len = 0, .cap = MAX_RESP_BYTES };

    esp_http_client_config_t cfg = {
        .url           = url,
        .event_handler = http_evt,
        .user_data     = &r,
        .timeout_ms    = 8000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (token && token[0]) {
        esp_http_client_set_header(client, "X-RLCD-Token", token);
    }
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (err != ESP_OK || status / 100 != 2) {
        ESP_LOGW(TAG, "GET failed err=%s status=%d", esp_err_to_name(err), status);
        free(buf);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return ESP_ERR_INVALID_RESPONSE;

    cJSON *upd = cJSON_GetObjectItem(root, "updated_at");
    if (cJSON_IsString(upd)) {
        strncpy(out->updated_at, upd->valuestring, sizeof(out->updated_at) - 1);
    }
    out->stale = cJSON_IsTrue(cJSON_GetObjectItem(root, "stale"));

    cJSON *claude = cJSON_GetObjectItem(root, "claude");
    if (!claude) { cJSON_Delete(root); return ESP_ERR_INVALID_RESPONSE; }

    cJSON *ab = cJSON_GetObjectItem(claude, "active_block");
    if (cJSON_IsObject(ab)) {
        out->active_block.valid = true;
        cJSON *t = cJSON_GetObjectItem(ab, "tokens_used");
        cJSON *c = cJSON_GetObjectItem(ab, "cost_usd");
        cJSON *m = cJSON_GetObjectItem(ab, "minutes_remaining");
        cJSON *p = cJSON_GetObjectItem(ab, "percent_used");
        out->active_block.tokens_used        = cJSON_IsNumber(t) ? (int64_t) t->valuedouble : 0;
        out->active_block.cost_usd           = cJSON_IsNumber(c) ? c->valuedouble : 0.0;
        out->active_block.minutes_remaining  = cJSON_IsNumber(m) ? (int32_t) m->valueint : 0;
        out->active_block.percent_used_x100  = pct_x100(p);
    }

    parse_bucket(cJSON_GetObjectItem(claude, "weekly"),   &out->weekly);
    parse_bucket(cJSON_GetObjectItem(claude, "today"),    &out->today);
    parse_bucket(cJSON_GetObjectItem(claude, "month"),    &out->month);
    parse_bucket(cJSON_GetObjectItem(claude, "lifetime"), &out->lifetime);

    cJSON *bm = cJSON_GetObjectItem(claude, "by_model");
    if (cJSON_IsArray(bm)) {
        int n = cJSON_GetArraySize(bm);
        if (n > USAGE_MAX_MODELS) n = USAGE_MAX_MODELS;
        for (int i = 0; i < n; ++i) {
            cJSON *m = cJSON_GetArrayItem(bm, i);
            cJSON *name = cJSON_GetObjectItem(m, "model");
            cJSON *tok  = cJSON_GetObjectItem(m, "tokens");
            cJSON *cst  = cJSON_GetObjectItem(m, "cost_usd");
            if (cJSON_IsString(name)) {
                strncpy(out->models[i].model, name->valuestring, sizeof(out->models[i].model) - 1);
            }
            out->models[i].tokens   = cJSON_IsNumber(tok) ? (int64_t) tok->valuedouble : 0;
            out->models[i].cost_usd = cJSON_IsNumber(cst) ? cst->valuedouble : 0.0;
        }
        out->model_count = n;
    }

    cJSON_Delete(root);
    return ESP_OK;
}
