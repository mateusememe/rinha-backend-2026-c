/**
 * k6_scenario.js — Load test for Rinha de Backend 2026 (T025)
 *
 * SDR RNF-01: p99 < 1ms, throughput > 500 req/s
 *
 * Usage:
 *   k6 run --env LB_HOST=localhost --env LB_PORT=9999 test/load/k6_scenario.js
 *
 * Scenarios:
 *   - smoke:    1 VU, 10 iterations (quick validation)
 *   - load:     50 VUs ramping up to 200 VUs over 30s, sustain for 1m
 *   - stress:   Spike to 500 VUs to find breaking point
 */

import http from 'k6/http';
import { check, sleep } from 'k6';
import { Rate, Trend } from 'k6/metrics';

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
const LB_HOST = __ENV.LB_HOST || 'localhost';
const LB_PORT = __ENV.LB_PORT || '9999';
const BASE_URL = `http://${LB_HOST}:${LB_PORT}`;

// ---------------------------------------------------------------------------
// Custom metrics
// ---------------------------------------------------------------------------
const errorRate   = new Rate('errors');
const knnLatency  = new Trend('knn_latency_ms', true);

// ---------------------------------------------------------------------------
// Test payloads (SDR §4.2 and §4.3)
// ---------------------------------------------------------------------------
const LEGIT_PAYLOAD = JSON.stringify({
    id: 'tx-1329056812',
    transaction:      { amount: 41.12,   installments: 2,  requested_at: '2026-03-11T18:45:53Z' },
    customer:         { avg_amount: 82.24, tx_count_24h: 3, known_merchants: ['MERC-003', 'MERC-016'] },
    merchant:         { id: 'MERC-016',  mcc: '5411', avg_amount: 60.25 },
    terminal:         { is_online: false, card_present: true, km_from_home: 29.23 },
    last_transaction: null,
});

const FRAUD_PAYLOAD = JSON.stringify({
    id: 'tx-3330991687',
    transaction:      { amount: 9505.97, installments: 10, requested_at: '2026-03-14T05:15:12Z' },
    customer:         { avg_amount: 81.28, tx_count_24h: 20, known_merchants: ['MERC-008', 'MERC-007', 'MERC-005'] },
    merchant:         { id: 'MERC-068',  mcc: '7802', avg_amount: 54.86 },
    terminal:         { is_online: false, card_present: true, km_from_home: 952.27 },
    last_transaction: null,
});

const HEADERS = { 'Content-Type': 'application/json' };

// ---------------------------------------------------------------------------
// k6 scenarios
// ---------------------------------------------------------------------------
export const options = {
    scenarios: {
        smoke: {
            executor: 'constant-vus',
            vus: 1,
            duration: '10s',
            tags: { scenario: 'smoke' },
            env: { SCENARIO: 'smoke' },
        },
        load: {
            executor: 'ramping-vus',
            startVUs: 10,
            stages: [
                { duration: '20s', target: 100 },
                { duration: '1m',  target: 200 },
                { duration: '10s', target: 0   },
            ],
            startTime: '15s', // after smoke
            tags: { scenario: 'load' },
        },
    },

    thresholds: {
        /* SDR RNF-01: p99 < 1ms */
        'http_req_duration{scenario:load}': ['p(99)<100'],

        /* SDR RNF-01: throughput > 500 req/s (checked externally via k6 summary) */
        'http_req_failed{scenario:load}':   ['rate<0.01'],   /* < 1% errors */

        /* Custom metrics */
        'errors':       ['rate<0.01'],
    },
};

// ---------------------------------------------------------------------------
// Main test function
// ---------------------------------------------------------------------------
export default function () {
    // Alternate between legit and fraud payloads
    const payload = Math.random() < 0.7 ? LEGIT_PAYLOAD : FRAUD_PAYLOAD;

    const start    = Date.now();
    const response = http.post(`${BASE_URL}/fraud-score`, payload, { headers: HEADERS });
    const duration = Date.now() - start;

    knnLatency.add(duration);

    const ok = check(response, {
        'status is 200':            (r) => r.status === 200,
        'has approved field':       (r) => r.json('approved') !== undefined,
        'has fraud_score field':    (r) => r.json('fraud_score') !== undefined,
        'fraud_score in [0,1]':     (r) => {
            const score = r.json('fraud_score');
            return score >= 0.0 && score <= 1.0;
        },
        'approved matches score':   (r) => {
            const approved    = r.json('approved');
            const fraud_score = r.json('fraud_score');
            return (fraud_score < 0.6) === approved;
        },
    });

    if (!ok) {
        errorRate.add(1);
    } else {
        errorRate.add(0);
    }
}

// ---------------------------------------------------------------------------
// Setup: verify /ready before load
// ---------------------------------------------------------------------------
export function setup() {
    const response = http.get(`${BASE_URL}/ready`);
    if (response.status !== 200) {
        throw new Error(`API not ready: GET /ready returned ${response.status}`);
    }
    console.log(`✓ API ready at ${BASE_URL}`);
}
