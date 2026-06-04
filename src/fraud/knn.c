#include "knn.h"
#include <limits.h>
#include <string.h>
#include "vectorize.h"

#if defined(__aarch64__) || defined(_M_ARM64)
    #include <arm_neon.h>
#elif defined(__x86_64__) || defined(_M_X64)
    #include <immintrin.h>
#endif

static inline int32_t dist_sq_simd_v(const int16_t *q, const int16_t *b) {
#if defined(__aarch64__) || defined(_M_ARM64)
    int16x8_t vq1 = vld1q_s16(q);
    int16x8_t vq2 = vld1q_s16(q + 8);
    int16x8_t vb1 = vld1q_s16(b);
    int16x8_t vb2 = vld1q_s16(b + 8);
    int16x8_t diff1 = vsubq_s16(vq1, vb1);
    int16x8_t diff2 = vsubq_s16(vq2, vb2);
    int32x4_t sq = vmull_s16(vget_low_s16(diff1), vget_low_s16(diff1));
    sq = vmlal_s16(sq, vget_high_s16(diff1), vget_high_s16(diff1));
    sq = vmlal_s16(sq, vget_low_s16(diff2), vget_low_s16(diff2));
    sq = vmlal_s16(sq, vget_high_s16(diff2), vget_high_s16(diff2));
    return vaddvq_s32(sq);
#elif defined(__x86_64__) || defined(_M_X64)
    // AVX2 implementation for x86_64
    __m256i vq = _mm256_loadu_si256((const __m256i*)q);
    __m256i vb = _mm256_loadu_si256((const __m256i*)b);
    __m256i diff = _mm256_sub_epi16(vq, vb);
    
    // Squared differences (madd_epi16 performs (a*b) + (c*d) for pairs)
    __m256i sq_pairs = _mm256_madd_epi16(diff, diff);
    
    // Horizontal sum of the 8 int32 results
    __m128i low = _mm256_castsi256_si128(sq_pairs);
    __m128i high = _mm256_extracti128_si256(sq_pairs, 1);
    __m128i sum128 = _mm_add_epi32(low, high);
    sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_SHUFFLE(1, 0, 3, 2)));
    sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_SHUFFLE(2, 3, 0, 1)));
    return _mm_cvtsi128_si32(sum128);
#else
    // Fallback scalar
    int32_t d2 = 0;
    for (int i = 0; i < 16; i++) {
        int32_t diff = q[i] - b[i];
        d2 += diff * diff;
    }
    return d2;
#endif
}

void knn_search(const ref_store_t *store, const rinha_vec_t query, int limit, knn_result_t *result) {
    int32_t best_dists[9] = {INT_MAX, INT_MAX, INT_MAX, INT_MAX, INT_MAX, INT_MAX, INT_MAX, INT_MAX, INT_MAX};
    uint8_t best_labels[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    
    int q_key = get_bucket_key(query);
    const uint16_t *order = &store->neighbor_orders[q_key * BUCKET_SEARCH_LIMIT];
    
    // unused variables removed
    
    for (int i = 0; i < limit; i++) {
        uint16_t b_idx = order[i];
        uint32_t start = store->buckets[b_idx].start_idx;
        uint32_t count = store->buckets[b_idx].count;
        
        const ref_record_t *records = &store->records[start];
        for (uint32_t j = 0; j < count; j++) {
            __builtin_prefetch(&records[j+16], 0, 0);
            
            int32_t d2 = dist_sq_simd_v(query, records[j].dims);
            
            if (d2 < best_dists[8]) {
                int pos = 8;
                while (pos > 0 && d2 < best_dists[pos-1]) {
                    best_dists[pos] = best_dists[pos-1];
                    best_labels[pos] = best_labels[pos-1];
                    pos--;
                }
                best_dists[pos] = d2;
                best_labels[pos] = records[j].is_fraud;
            }
        }
    }

    // Unweighted voting among top 5 nearest neighbors
    int fraud_count = 0;
    for (int i = 0; i < 5; i++) {
        if (best_dists[i] != INT_MAX && best_labels[i]) {
            fraud_count++;
        }
    }
    
    result->fraud_score = (float)fraud_count / 5.0f;
    result->approved = (fraud_count < 3); // Majority vote (3 or more fraud -> not approved)
}
