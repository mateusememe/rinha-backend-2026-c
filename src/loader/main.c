#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include "common/types.h"
#include "common/shm.h"
#include "fraud/vectorize.h"

#define READ_BUF_SIZE (512 * 1024)

static void build_neighbor_orders(ref_store_t *store) {
    uint8_t *seen = malloc(NUM_BUCKETS);
    for (int bucket_key = 0; bucket_key < NUM_BUCKETS; bucket_key++) {
        memset(seen, 0, NUM_BUCKETS);
        int amount = bucket_key & 7;
        int ratio = (bucket_key >> 3) & 7;
        int km_home = (bucket_key >> 6) & 7;
        int hour = (bucket_key >> 9) & 3;
        int no_last = (bucket_key >> 11) & 1;
        
        int count = 0;
        for (int radius = 0; radius < 8 && count < BUCKET_SEARCH_LIMIT; radius++) {
            for (int a = (amount - radius < 0 ? 0 : amount - radius); a <= (amount + radius > 7 ? 7 : amount + radius) && count < BUCKET_SEARCH_LIMIT; a++) {
                for (int r = (ratio - radius < 0 ? 0 : ratio - radius); r <= (ratio + radius > 7 ? 7 : ratio + radius) && count < BUCKET_SEARCH_LIMIT; r++) {
                    for (int k = (km_home - radius < 0 ? 0 : km_home - radius); k <= (km_home + radius > 7 ? 7 : km_home + radius) && count < BUCKET_SEARCH_LIMIT; k++) {
                        for (int h = (hour - radius < 0 ? 0 : hour - radius); h <= (hour + radius > 3 ? 3 : hour + radius) && count < BUCKET_SEARCH_LIMIT; h++) {
                            int last_start = (radius >= 2 ? 0 : no_last);
                            int last_end = (radius >= 2 ? 1 : no_last);
                            for (int last = last_start; last <= last_end && count < BUCKET_SEARCH_LIMIT; last++) {
                                int key = a | (r << 3) | (k << 6) | (h << 9) | (last << 11);
                                if (seen[key]) continue;
                                seen[key] = 1;
                                store->neighbor_orders[bucket_key * BUCKET_SEARCH_LIMIT + count++] = (uint16_t)key;
                            }
                        }
                    }
                }
            }
        }
    }
    free(seen);
}

static bool parse_ref_item(const char *v, rinha_vec_t out, bool *is_fraud) {
    const char *v_start = strstr(v, "\"vector\":[");
    if (!v_start) return false;
    v_start += 10;
    const char *p = v_start;
    for (int i = 0; i < 14; i++) {
        char *end;
        double val = strtod(p, &end);
        out[i] = (int16_t)round(val * SCALE_FACTOR);
        p = end;
        if (*p == ',') p++;
    }
    out[14] = 0; out[15] = 0;
    *is_fraud = (strstr(v, "\"label\":\"fraud\"") != NULL);
    return true;
}

int main() {
    printf("Starting data-loader (AMD64 built)...\n");
    ref_store_t *store = shm_create();
    if (!store) {
        fprintf(stderr, "FATAL: Failed to create SHM. Error: %s\n", strerror(errno));
        return 1;
    }

    const char *data_path = "/app/resources/references.json";
    FILE *f = fopen(data_path, "r");
    if (!f) { 
        fprintf(stderr, "FATAL: Could not open %s. Error: %s\n", data_path, strerror(errno)); 
        return 1; 
    }

    char *buf = malloc(READ_BUF_SIZE);
    uint32_t count = 0;
    uint32_t *bucket_counts = calloc(NUM_BUCKETS, sizeof(uint32_t));
    
    printf("Streaming dataset into memory...\n");
    
    size_t leftover = 0;
    while (1) {
        size_t n = fread(buf + leftover, 1, READ_BUF_SIZE - leftover - 1, f);
        if (n == 0 && leftover == 0) break;
        size_t total = n + leftover;
        buf[total] = '\0';

        char *curr = buf;
        while (count < MAX_REFS) {
            char *obj_start = strstr(curr, "{\"vector\":[");
            if (!obj_start) break;
            
            char *obj_end = strchr(obj_start, '}');
            if (!obj_end) break;

            *obj_end = '\0';
            bool is_fraud;
            if (parse_ref_item(obj_start, store->records[count].dims, &is_fraud)) {
                store->records[count].is_fraud = (uint8_t)is_fraud;
                int key = get_bucket_key(store->records[count].dims);
                bucket_counts[key]++;
                count++;
                if (count % 250000 == 0) printf("Loaded %u records...\n", count);
            }
            curr = obj_end + 1;
        }
        leftover = total - (curr - buf);
        if (leftover > 0) memmove(buf, curr, leftover);
        if (n == 0) break;
    }
    store->count = count;
    fclose(f);
    free(buf);

    if (count == 0) {
        fprintf(stderr, "FATAL: No records loaded from dataset!\n");
        return 1;
    }

    printf("Sorting %u records into buckets...\n", count);
    ref_record_t *sorted = malloc(sizeof(ref_record_t) * MAX_REFS);
    if (!sorted) { 
        fprintf(stderr, "FATAL: OOM allocating sorting buffer (%zu bytes)\n", sizeof(ref_record_t) * MAX_REFS); 
        return 1; 
    }
    
    uint32_t current_pos = 0;
    for (int i = 0; i < NUM_BUCKETS; i++) {
        store->buckets[i].start_idx = current_pos;
        current_pos += bucket_counts[i];
        store->buckets[i].count = 0;
    }

    for (uint32_t i = 0; i < count; i++) {
        int key = get_bucket_key(store->records[i].dims);
        uint32_t pos = store->buckets[key].start_idx + store->buckets[key].count;
        sorted[pos] = store->records[i];
        store->buckets[key].count++;
    }
    
    memcpy(store->records, sorted, sizeof(ref_record_t) * count);
    free(sorted);
    free(bucket_counts);

    printf("Finalizing build...\n");
    build_neighbor_orders(store);

    store->magic = SHM_MAGIC;
    printf("Success! %u records ready in SHM.\n", store->count);
    return 0;
}
