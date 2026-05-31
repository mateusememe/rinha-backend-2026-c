#ifndef RINHA_TYPES_H
#define RINHA_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#define SCALE_FACTOR 10000
#define PACKED_DIM 16
#define MAX_REFS 1000000
#define SHM_NAME "/rinha_refstore_v5"
#define SHM_MAGIC 0x52494E4841323035ULL
#define NUM_BUCKETS 4096
#define BUCKET_SEARCH_LIMIT 32

typedef int16_t rinha_vec_t[PACKED_DIM];

typedef struct {
    rinha_vec_t dims; // 32 bytes
    uint8_t is_fraud; // 1 byte
    uint8_t _pad[31]; // Pad to 64 bytes for cache line alignment
} ref_record_t;

typedef struct {
    uint32_t start_idx;
    uint32_t count;
} bucket_t;

typedef struct {
    uint64_t magic;
    uint32_t count;
    bucket_t buckets[NUM_BUCKETS];
    uint16_t neighbor_orders[NUM_BUCKETS * BUCKET_SEARCH_LIMIT];
    uint8_t _header_pad[52]; // Padding to align records to 64-byte boundary
    ref_record_t records[MAX_REFS];
} ref_store_t;

typedef struct {
    uint32_t amount_m;
    uint8_t installments;
    char requested_at[24];
    uint32_t customer_avg_amount_m;
    uint8_t customer_tx_count_24h;
    uint32_t known_merchant_hashes[32];
    uint8_t known_merchant_count;
    uint32_t merchant_id_hash;
    uint16_t merchant_mcc;
    uint32_t merchant_avg_amount_m;
    bool terminal_is_online;
    bool terminal_card_present;
    uint32_t terminal_km_from_home_m;
    bool has_last_tx;
    char last_tx_timestamp[24];
    uint32_t last_tx_km_from_current_m;
} tx_payload_t;

typedef struct {
    float fraud_score;
    bool approved;
} knn_result_t;

#endif
