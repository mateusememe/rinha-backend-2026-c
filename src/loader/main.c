#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>
#include "common/types.h"
#include "common/shm.h"
#include "fraud/vectorize.h"

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
    v = strstr(v, "\"vector\":[");
    if (!v) return false;
    v += 10;
    for (int i = 0; i < 14; i++) {
        char *end;
        double val = strtod(v, &end);
        out[i] = (int16_t)round(val * SCALE_FACTOR);
        v = end;
        if (*v == ',') v++;
    }
    out[14] = 0; out[15] = 0;
    *is_fraud = (strstr(v, "\"label\":\"fraud\"") != NULL);
    // Note: this strstr might look ahead too far if not careful, 
    // but reference format has label after vector.
    return true;
}

int main() {
    ref_store_t *store = shm_create();
    if (!store) {
        fprintf(stderr, "Failed to create SHM\n");
        return 1;
    }

    FILE *f = fopen("resources/references.json", "r");
    if (!f) { perror("fopen references"); return 1; }

    char *line = NULL;
    size_t len = 0;
    uint32_t count = 0;
    uint32_t *bucket_counts = calloc(NUM_BUCKETS, sizeof(uint32_t));
    
    printf("Loading references...\n");
    if (getline(&line, &len, f) > 0) {
        const char *p = line;
        while ((p = strstr(p, "{\"vector\":[")) && count < MAX_REFS) {
            bool is_fraud;
            if (parse_ref_item(p, store->records[count].dims, &is_fraud)) {
                store->records[count].is_fraud = (uint8_t)is_fraud;
                int key = get_bucket_key(store->records[count].dims);
                bucket_counts[key]++;
                count++;
                if (count % 100000 == 0) printf("Loaded %u...\n", count);
            }
            p += 10; // Skip the match start
        }
    }
    store->count = count;
    fclose(f);

    printf("Sorting %u records into buckets...\n", count);
    ref_record_t *sorted = malloc(sizeof(ref_record_t) * count);
    uint32_t current_pos = 0;
    for (int i = 0; i < NUM_BUCKETS; i++) {
        store->buckets[i].start_idx = current_pos;
        store->buckets[i].count = 0;
        current_pos += bucket_counts[i];
    }

    uint32_t *bucket_offsets = calloc(NUM_BUCKETS, sizeof(uint32_t));
    for (uint32_t i = 0; i < count; i++) {
        int key = get_bucket_key(store->records[i].dims);
        uint32_t pos = store->buckets[key].start_idx + bucket_offsets[key];
        sorted[pos] = store->records[i];
        bucket_offsets[key]++;
        store->buckets[key].count++;
    }
    
    memcpy(store->records, sorted, sizeof(ref_record_t) * count);
    free(sorted);
    free(bucket_counts);
    free(bucket_offsets);

    uint32_t max_count = 0;
    for (int i = 0; i < NUM_BUCKETS; i++) {
        if (store->buckets[i].count > max_count) max_count = store->buckets[i].count;
    }
    printf("Max bucket size: %u\n", max_count);

    printf("Building neighbor orders...\n");
    build_neighbor_orders(store);

    store->magic = SHM_MAGIC;
    printf("Done. Total records: %u, Magic: %llx\n", store->count, (unsigned long long)store->magic);
    return 0;
}
