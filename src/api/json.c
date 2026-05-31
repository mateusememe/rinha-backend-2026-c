#define _GNU_SOURCE
#include "json.h"
#include "common/utils.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

static inline const char* skip_ws(const char* p) {
    if (!p) return NULL;
    while (*p && isspace(*p)) p++;
    return p;
}

static uint32_t parse_milli(const char* p) {
    if (!p) return 0;
    while (*p && !isdigit(*p) && *p != '-') p++;
    long long integral = atoll(p);
    while (*p && isdigit(*p)) p++;
    long long fractional = 0;
    if (*p == '.') {
        p++;
        int digits = 0;
        while (digits < 3 && isdigit(*p)) {
            fractional = fractional * 10 + (*p - '0');
            p++; digits++;
        }
        while (digits < 3) { fractional *= 10; digits++; }
    }
    return (uint32_t)(integral * 1000 + fractional);
}

static const char* find_key(const char* p, const char* key) {
    if (!p) return NULL;
    return strstr(p, key);
}

static void copy_quoted(const char* src, char* dst, size_t max_len) {
    if (!src) return;
    const char* start = strchr(src, '"');
    if (!start) return;
    start++;
    const char* end = strchr(start, '"');
    if (!end) return;
    size_t len = end - start;
    if (len >= max_len) len = max_len - 1;
    memcpy(dst, start, len);
    dst[len] = '\0';
}

bool parse_transaction_json(const char *json, size_t len, tx_payload_t *out) {
    (void)len;
    memset(out, 0, sizeof(tx_payload_t));
    
    const char* tx = find_key(json, "\"transaction\"");
    if (tx) {
        out->amount_m = parse_milli(find_key(tx, "\"amount\""));
        const char* inst = find_key(tx, "\"installments\"");
        if (inst) out->installments = (uint8_t)atoi(skip_ws(strchr(inst, ':') + 1));
        copy_quoted(find_key(tx, "\"requested_at\""), out->requested_at, sizeof(out->requested_at));
    }

    const char* cust = find_key(json, "\"customer\"");
    if (cust) {
        out->customer_avg_amount_m = parse_milli(find_key(cust, "\"avg_amount\""));
        const char* txc = find_key(cust, "\"tx_count_24h\"");
        if (txc) out->customer_tx_count_24h = (uint8_t)atoi(skip_ws(strchr(txc, ':') + 1));
        const char* km = find_key(cust, "\"known_merchants\"");
        if (km) {
            km = strchr(km, '[');
            if (km) {
                km++;
                while (*km && *km != ']') {
                    km = skip_ws(km);
                    if (*km == '"') {
                        km++;
                        const char* end = strchr(km, '"');
                        if (end && out->known_merchant_count < 32) {
                            out->known_merchant_hashes[out->known_merchant_count++] = fnv1a_hash(km, end - km);
                            km = end + 1;
                        }
                    }
                    while (*km && *km != ',' && *km != ']') km++;
                    if (*km == ',') km++;
                }
            }
        }
    }

    const char* merc = find_key(json, "\"merchant\"");
    if (merc) {
        const char* mcc_p = find_key(merc, "\"mcc\"");
        if (mcc_p) out->merchant_mcc = (uint16_t)atoi(skip_ws(strchr(mcc_p, ':') + 1));
        out->merchant_avg_amount_m = parse_milli(find_key(merc, "\"avg_amount\""));
        const char* mid = find_key(merc, "\"id\"");
        if (mid) {
            const char* v = strchr(strchr(mid, ':'), '"');
            if (v) {
                const char* end = strchr(v + 1, '"');
                if (end) out->merchant_id_hash = fnv1a_hash(v + 1, end - (v + 1));
            }
        }
    }

    const char* term = find_key(json, "\"terminal\"");
    if (term) {
        out->terminal_km_from_home_m = parse_milli(find_key(term, "\"km_from_home\""));
        const char* online = find_key(term, "\"is_online\"");
        if (online) out->terminal_is_online = (strncmp(skip_ws(strchr(online, ':') + 1), "true", 4) == 0);
        const char* card = find_key(term, "\"card_present\"");
        if (card) out->terminal_card_present = (strncmp(skip_ws(strchr(card, ':') + 1), "true", 4) == 0);
    }

    const char* last = find_key(json, "\"last_transaction\"");
    if (last) {
        const char* v = skip_ws(strchr(last, ':') + 1);
        if (v && strncmp(v, "null", 4) != 0) {
            out->has_last_tx = true;
            out->last_tx_km_from_current_m = parse_milli(find_key(last, "\"km_from_current\""));
            copy_quoted(find_key(last, "\"requested_at\""), out->last_tx_timestamp, sizeof(out->last_tx_timestamp));
        }
    }

    return true; 
}

void serialize_knn_result(const knn_result_t *res, char *out, size_t max_len) {
    snprintf(out, max_len, "{\"approved\":%s,\"fraud_score\":%.1f}", 
        res->approved ? "true" : "false", res->fraud_score);
}
