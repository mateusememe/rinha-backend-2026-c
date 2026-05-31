#include "knn.h"
#include <arm_neon.h>
#include <limits.h>
#include <string.h>
#include "vectorize.h"

static inline int32_t dist_sq_simd_v(int16x8_t vq1, int16x8_t vq2, const int16_t *b) {
    int16x8_t vb1 = vld1q_s16(b);
    int16x8_t vb2 = vld1q_s16(b + 8);
    int16x8_t diff1 = vsubq_s16(vq1, vb1);
    int16x8_t diff2 = vsubq_s16(vq2, vb2);
    int32x4_t sq = vmull_s16(vget_low_s16(diff1), vget_low_s16(diff1));
    sq = vmlal_s16(sq, vget_high_s16(diff1), vget_high_s16(diff1));
    sq = vmlal_s16(sq, vget_low_s16(diff2), vget_low_s16(diff2));
    sq = vmlal_s16(sq, vget_high_s16(diff2), vget_high_s16(diff2));
    return vaddvq_s32(sq);
}

void knn_search(const ref_store_t *store, const rinha_vec_t query, knn_result_t *result) {
    int32_t best_dists[5] = {INT_MAX, INT_MAX, INT_MAX, INT_MAX, INT_MAX};
    uint8_t best_labels[5] = {0, 0, 0, 0, 0};
    
    int q_key = get_bucket_key(query);
    const uint16_t *order = &store->neighbor_orders[q_key * BUCKET_SEARCH_LIMIT];
    
    int16x8_t vq1 = vld1q_s16(query);
    int16x8_t vq2 = vld1q_s16(query + 8);
    
    uint32_t total_searched = 0;
    
    for (int i = 0; i < BUCKET_SEARCH_LIMIT; i++) {
        uint16_t b_idx = order[i];
        uint32_t start = store->buckets[b_idx].start_idx;
        uint32_t count = store->buckets[b_idx].count;
        
        const ref_record_t *records = &store->records[start];
        for (uint32_t j = 0; j < count; j++) {
            // Manual prefetch
            __builtin_prefetch(&records[j+16], 0, 0);
            
            int32_t d2 = dist_sq_simd_v(vq1, vq2, records[j].dims);
            
            if (d2 == 0) {
                result->fraud_score = records[j].is_fraud ? 1.0f : 0.0f;
                result->approved = !records[j].is_fraud;
                return;
            }
            
            if (d2 < best_dists[4]) {
                int pos = 4;
                while (pos > 0 && d2 < best_dists[pos-1]) {
                    best_dists[pos] = best_dists[pos-1];
                    best_labels[pos] = best_labels[pos-1];
                    pos--;
                }
                best_dists[pos] = d2;
                best_labels[pos] = records[j].is_fraud;
            }
            
            total_searched++;
            // Check early exit every 1024 records
            if ((total_searched & 1023) == 0) {
                if (total_searched > 8000 && best_dists[4] != INT_MAX) goto finish;
                // Strong decision check
                int frauds = 0;
                for (int k = 0; k < 5; k++) if (best_labels[k]) frauds++;
                if (best_dists[4] != INT_MAX && (frauds == 5 || frauds == 0)) {
                    result->fraud_score = (float)frauds / 5.0f;
                    result->approved = (result->fraud_score < 0.6f);
                    return;
                }
            }
        }
    }

finish:;
    int frauds = 0;
    for (int i = 0; i < 5; i++) if (best_labels[i]) frauds++;
    result->fraud_score = (float)frauds / 5.0f;
    result->approved = (result->fraud_score < 0.6f);
}
