#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

int main() {
    ref_store_t *store = shm_create();
    if (!store) return 1;

    FILE *f = fopen("resources/references.json", "r");
    if (!f) return 1;

    char *line = NULL;
    size_t len = 0;
    uint32_t count = 0;
    uint32_t *bucket_counts = calloc(NUM_BUCKETS, sizeof(uint32_t));

    while (getline(&line, &len, f) != -1 && count < MAX_REFS) {
        char *v_start = strstr(line, "\"vector\":[");
        if (!v_start) continue;
        v_start += 10;
        char *p = v_start;
        for (int i = 0; i < 14; i++) {
            char *end;
            store->records[count].dims[i] = (int16_t)round(strtod(p, &end) * SCALE_FACTOR);
            p = end; if (*p == ',') p++;
        }
        store->records[count].dims[14] = 0;
        store->records[count].dims[15] = 0;
        store->records[count].is_fraud = (strstr(line, "\"label\":\"fraud\"") != NULL);
        
        int key = get_bucket_key(store->records[count].dims);
        bucket_counts[key]++;
        count++;
    }
    store->count = count;
    free(line);
    fclose(f);

    ref_record_t *sorted = malloc(sizeof(ref_record_t) * count);
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

    build_neighbor_orders(store);
    store->magic = SHM_MAGIC;
    printf("Loaded %u records\n", store->count);
    return 0;
}
