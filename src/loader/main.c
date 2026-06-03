#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <zlib.h>
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

// Streaming parser: reads directly from gzip, parses record-by-record
// without loading the entire 284MB JSON into memory.
// The JSON format is: [{"vector":[...], "label":"..."}, {"vector":[...], ...}, ...]
// All on a single line.

#define READ_BUF_SIZE 65536

typedef struct {
    gzFile gz;
    char buf[READ_BUF_SIZE];
    int pos;
    int len;
} stream_t;

static int stream_init(stream_t *s, const char *path) {
    s->gz = gzopen(path, "rb");
    if (!s->gz) return -1;
    s->pos = 0;
    s->len = 0;
    return 0;
}

static inline int stream_peek(stream_t *s) {
    if (s->pos >= s->len) {
        s->len = gzread(s->gz, s->buf, READ_BUF_SIZE);
        s->pos = 0;
        if (s->len <= 0) return -1;
    }
    return (unsigned char)s->buf[s->pos];
}

static inline int stream_next(stream_t *s) {
    int c = stream_peek(s);
    if (c >= 0) s->pos++;
    return c;
}

static inline void stream_skip_ws(stream_t *s) {
    int c;
    while ((c = stream_peek(s)) >= 0 && (c == ' ' || c == '\t' || c == '\n' || c == '\r'))
        s->pos++;
}

// Skip until we find the target string in the stream
// Returns 0 on success, -1 on EOF
static int stream_find(stream_t *s, const char *target) {
    int tlen = strlen(target);
    int matched = 0;
    int c;
    while ((c = stream_next(s)) >= 0) {
        if (c == target[matched]) {
            matched++;
            if (matched == tlen) return 0;
        } else {
            // Reset, but check if current char starts the pattern
            if (c == target[0]) matched = 1;
            else matched = 0;
        }
    }
    return -1;
}

// Parse a float from the stream into the provided buffer
static double stream_parse_float(stream_t *s) {
    char numbuf[32];
    int i = 0;
    stream_skip_ws(s);
    int c;
    while ((c = stream_peek(s)) >= 0 && i < 30) {
        if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == 'e' || c == 'E' || c == '+') {
            numbuf[i++] = (char)c;
            s->pos++;
        } else {
            break;
        }
    }
    numbuf[i] = '\0';
    return atof(numbuf);
}

int main() {
    ref_store_t *store = shm_create();
    if (!store) {
        fprintf(stderr, "Failed to create shared memory\n");
        return 1;
    }

    // Try to read directly from gzip to avoid decompressing the full 284MB file to disk
    const char *gz_paths[] = {
        "/dataset_up/resources/references.json.gz",
        "/dataset_here/resources/references.json.gz",
        "resources/references.json.gz",
        "/app/resources/references.json.gz",
        NULL
    };

    stream_t stream;
    int found = 0;
    for (int i = 0; gz_paths[i]; i++) {
        if (stream_init(&stream, gz_paths[i]) == 0) {
            fprintf(stderr, "Reading from %s\n", gz_paths[i]);
            found = 1;
            break;
        }
    }

    // Fallback: try uncompressed file
    if (!found) {
        const char *json_paths[] = {
            "resources/references.json",
            "/app/resources/references.json",
            NULL
        };
        for (int i = 0; json_paths[i]; i++) {
            if (stream_init(&stream, json_paths[i]) == 0) {
                fprintf(stderr, "Reading from %s\n", json_paths[i]);
                found = 1;
                break;
            }
        }
    }

    if (!found) {
        fprintf(stderr, "ERROR: Could not find references file\n");
        return 1;
    }

    uint32_t count = 0;
    uint32_t *bucket_counts = calloc(NUM_BUCKETS, sizeof(uint32_t));

    // Parse records: find each "vector":[ then parse 14 floats, then find "label":"
    while (count < MAX_REFS) {
        // Find next vector
        if (stream_find(&stream, "\"vector\"") != 0) break;

        // Skip optional whitespace and colon
        stream_skip_ws(&stream);
        int c = stream_peek(&stream);
        if (c == ':') stream_next(&stream);
        stream_skip_ws(&stream);

        // Expect '['
        c = stream_peek(&stream);
        if (c != '[') continue;
        stream_next(&stream); // consume '['

        // Parse 14 float values
        for (int i = 0; i < 14; i++) {
            stream_skip_ws(&stream);
            store->records[count].dims[i] = (int16_t)round(stream_parse_float(&stream) * SCALE_FACTOR);
            stream_skip_ws(&stream);
            c = stream_peek(&stream);
            if (c == ',') stream_next(&stream);
        }
        store->records[count].dims[14] = 0;
        store->records[count].dims[15] = 0;

        // Find label
        if (stream_find(&stream, "\"label\"") != 0) break;
        stream_skip_ws(&stream);
        c = stream_peek(&stream);
        if (c == ':') stream_next(&stream);
        stream_skip_ws(&stream);

        // Read label value - check if it's "fraud"
        // Skip the opening quote
        c = stream_peek(&stream);
        if (c == '"') stream_next(&stream);
        // Check first char: 'f' = fraud, 'l' = legit
        c = stream_peek(&stream);
        store->records[count].is_fraud = (c == 'f') ? 1 : 0;

        int key = get_bucket_key(store->records[count].dims);
        bucket_counts[key]++;
        count++;

        if (count % 500000 == 0) {
            fprintf(stderr, "Parsed %u records...\n", count);
        }
    }
    gzclose(stream.gz);

    store->count = count;
    fprintf(stderr, "Parsed %u total records, sorting into buckets...\n", count);

    ref_record_t *sorted = malloc(sizeof(ref_record_t) * count);
    if (!sorted) {
        fprintf(stderr, "Failed to allocate sorted array (%zu bytes)\n", sizeof(ref_record_t) * count);
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

    build_neighbor_orders(store);
    store->magic = SHM_MAGIC;
    printf("Loaded %u records\n", store->count);
    return 0;
}
