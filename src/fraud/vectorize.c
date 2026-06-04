#include "vectorize.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

static inline int16_t quantize(double val) {
    if (val < 0.0) val = 0.0;
    if (val > 1.0) val = 1.0;
    return (int16_t)round(val * SCALE_FACTOR);
}

static inline bool try_parse_2(const char* s, int* val) {
    if (!s[0] || !s[1]) return false;
    *val = (s[0] - '0') * 10 + (s[1] - '0');
    return true;
}

static inline bool try_parse_4(const char* s, int* val) {
    if (!s[0] || !s[1] || !s[2] || !s[3]) return false;
    *val = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
    return true;
}

static inline bool try_parse_time(const char* s, int* y, int* m, int* d, int* h, int* min) {
    if (strlen(s) < 16) return false;
    if (!try_parse_4(s, y)) return false;
    if (!try_parse_2(s + 5, m)) return false;
    if (!try_parse_2(s + 8, d)) return false;
    if (!try_parse_2(s + 11, h)) return false;
    if (!try_parse_2(s + 14, min)) return false;
    return true;
}

static inline int day_of_week(int y, int m, int d) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y--;
    int dow = (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
    return (dow + 6) % 7; // Monday=0, Sunday=6
}

static inline long long days_from_civil(int y, int m, int d) {
    y -= (m <= 2);
    long long era = (y >= 0 ? y : y - 399) / 400;
    long long yoe = y - era * 400;
    long long mp = m + (m > 2 ? -3 : 9);
    long long doy = (153 * mp + 2) / 5 + d - 1;
    long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097LL + doe - 719468LL;
}

static inline long long epoch_minutes(int y, int m, int d, int h, int min) {
    return days_from_civil(y, m, d) * 1440LL + h * 60LL + min;
}

static double get_mcc_risk(uint16_t mcc) {
    switch(mcc) {
        case 5411: return 0.15;
        case 5812: return 0.30;
        case 5912: return 0.20;
        case 5944: return 0.45;
        case 7801: return 0.80;
        case 7802: return 0.75;
        case 7995: return 0.85;
        case 4511: return 0.35;
        case 5311: return 0.25;
        case 5999: return 0.50;
        default: return 0.50;
    }
}

void vectorize_payload(const tx_payload_t *payload, rinha_vec_t out) {
    memset(out, 0, sizeof(rinha_vec_t));
    
    int y, m, d, h, min;
    if (!try_parse_time(payload->requested_at, &y, &m, &d, &h, &min)) {
        y=2026; m=1; d=1; h=0; min=0;
    }
    int dow = day_of_week(y, m, d);

    out[0] = quantize((double)payload->amount_m / 10000000.0); // amount / 10000.0
    out[1] = quantize((double)payload->installments / 12.0);
    
    double ratio = (payload->customer_avg_amount_m <= 0) ? 1.0 : ((double)payload->amount_m / (double)payload->customer_avg_amount_m) / 10.0;
    out[2] = quantize(ratio) * 2; // Weight x2
    
    out[3] = quantize((double)h / 23.0);
    out[4] = quantize((double)dow / 6.0);

    if (payload->has_last_tx) {
        long long current = epoch_minutes(y, m, d, h, min);
        int ly, lm, ld, lh, lmin;
        long long last = try_parse_time(payload->last_tx_timestamp, &ly, &lm, &ld, &lh, &lmin)
            ? epoch_minutes(ly, lm, ld, lh, lmin)
            : current;
        long long diff = current - last;
        if (diff < 0) diff = 0;
        out[5] = quantize((double)diff / 1440.0) * 2; // Weight x2 for velocity
        out[6] = quantize((double)payload->last_tx_km_from_current_m / 1000000.0) * 2; // km / 1000.0, Weight x2
    } else {
        out[5] = -SCALE_FACTOR * 2;
        out[6] = -SCALE_FACTOR * 2;
    }

    out[7] = quantize((double)payload->terminal_km_from_home_m / 1000000.0) * 2; // Weight x2
    out[8] = quantize((double)payload->customer_tx_count_24h / 20.0);
    out[9] = payload->terminal_is_online ? SCALE_FACTOR : 0;
    out[10] = payload->terminal_card_present ? SCALE_FACTOR : 0;

    bool known = false;
    for (int i = 0; i < payload->known_merchant_count; i++) {
        if (payload->known_merchant_hashes[i] == payload->merchant_id_hash) { known = true; break; }
    }
    out[11] = known ? 0 : (SCALE_FACTOR * 2); // Inverted, Weight x2
    out[12] = quantize(get_mcc_risk(payload->merchant_mcc)) * 2; // Weight x2
    out[13] = quantize((double)payload->merchant_avg_amount_m / 10000000.0);
}

int get_bucket_key(const rinha_vec_t vector) {
    int amount = (vector[0] <= 0) ? 0 : (vector[0] * 8 / (SCALE_FACTOR + 1));
    if (amount > 7) amount = 7;
    int ratio = (vector[2] <= 0) ? 0 : (vector[2] * 8 / (SCALE_FACTOR + 1));
    if (ratio > 7) ratio = 7;
    int km_home = (vector[7] <= 0) ? 0 : (vector[7] * 8 / (SCALE_FACTOR + 1));
    if (km_home > 7) km_home = 7;
    int hour = (vector[3] <= 0) ? 0 : (vector[3] * 4 / (SCALE_FACTOR + 1));
    if (hour > 3) hour = 3;
    int no_last = (vector[5] < 0) ? 1 : 0;
    
    return amount | (ratio << 3) | (km_home << 6) | (hour << 9) | (no_last << 11);
}
