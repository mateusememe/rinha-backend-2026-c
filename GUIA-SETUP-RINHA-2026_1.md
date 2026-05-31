# Guia Completo: SpecKit + Gemini CLI + Antigravity
## Rinha de Backend 2026 — Implementação de Ponta a Ponta no macOS

---

## Índice

1. [Visão do Fluxo de Trabalho](#1-visão-do-fluxo-de-trabalho)
2. [Pré-requisitos do macOS](#2-pré-requisitos-do-macos)
3. [Instalar Gemini CLI](#3-instalar-gemini-cli)
4. [Instalar Antigravity IDE](#4-instalar-antigravity-ide)
5. [Instalar SpecKit na raiz do projeto](#5-instalar-speckit-na-raiz-do-projeto)
6. [Fluxo SDD de Ponta a Ponta](#6-fluxo-sdd-de-ponta-a-ponta)
7. [Prompts para cada fase do SpecKit](#7-prompts-para-cada-fase-do-speckit)
8. [Testar no macOS — Estratégia Correta](#8-testar-no-macos--estratégia-correta)
9. [Simular Limites de CPU/Memória Localmente](#9-simular-limites-de-cpumemória-localmente)
10. [Checklist de Submissão](#10-checklist-de-submissão)

---

## 1. Visão do Fluxo de Trabalho

```
SDD/SpecKit                 Ferramenta de Agente               Saída
────────────────────────────────────────────────────────────────────
/speckit.constitution  →    Gemini CLI ou Antigravity    →  constitution.md
/speckit.specify       →    Gemini CLI ou Antigravity    →  spec.md
/speckit.plan          →    Gemini CLI ou Antigravity    →  plan.md
/speckit.tasks         →    Gemini CLI ou Antigravity    →  tasks.md
/speckit.implement     →    Gemini CLI ou Antigravity    →  código C + Makefile + Dockerfile
                                                            docker-compose.yml
Manual / k6             →   Terminal macOS               →  testes de carga locais
Docker buildx           →   Terminal macOS               →  imagem linux/amd64
```

O Gemini CLI é o agente de terminal (sem GUI). O Antigravity é a IDE completa (tem GUI, Mission Control de agentes, terminal integrado). Use o Antigravity para a implementação principal e o Gemini CLI para tarefas pontuais no terminal.

---

## 2. Pré-requisitos do macOS

```bash
# 1. Xcode Command Line Tools (compilador C, git, make)
xcode-select --install

# 2. Homebrew (gerenciador de pacotes)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Adicione ao ~/.zshrc se Apple Silicon:
echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> ~/.zshrc
source ~/.zshrc

# 3. Node.js 20+ (necessário para SpecKit CLI e Gemini CLI)
brew install node
node --version   # deve ser v20+

# 4. Python 3.11+ e uv (necessário para SpecKit via uv)
brew install python
pip3 install uv

# 5. Docker Desktop (para containers)
brew install --cask docker
# Abra o Docker Desktop e aguarde inicializar

# 6. k6 (ferramenta de carga — usada pela Rinha)
brew install k6

# 7. wget (para healthchecks nos containers)
brew install wget

# 8. zlib (para o data-loader descomprimir references.json.gz)
brew install zlib

# Verificar tudo:
gcc --version
node --version
docker --version
k6 version
```

---

## 3. Instalar Gemini CLI

```bash
# Opção A — Homebrew (mais simples, recomendado)
brew install gemini-cli

# Opção B — npm global
npm install -g @google/gemini-cli

# Verificar instalação
gemini --version

# Autenticar (abre browser com OAuth Google)
gemini auth login
# ou configure via API key:
export GEMINI_API_KEY="sua_chave_do_ai_studio"
```

### Uso básico do Gemini CLI

```bash
# Modo interativo (REPL)
cd seu-projeto
gemini

# Modo one-shot
gemini -p "Explique este arquivo src/knn.c"

# Mudar modelo para Flash (mais rápido)
gemini -m gemini-2.5-flash -p "Revise meu Makefile"

# Ver arquivos do projeto com contexto
gemini -p "Leia src/ e me diga se há algum problema de alinhamento de memória"
```

---

## 4. Instalar Antigravity IDE

```bash
# Via Homebrew (recomendado — gerencia updates automaticamente)
brew install --cask antigravity
# ou: brew install antigravity (verifica qual está disponível)

# Alternativa: download direto
# Acesse antigravity.google/download
# Baixe "Antigravity.dmg" para Apple Silicon (ARM64) ou Intel (x64)
# Abra o .dmg e arraste para /Applications

# Instalar CLI (para abrir do terminal)
# No primeiro launch do Antigravity:
# Settings → Command Line → Install 'agy' command
# Depois:
agy .           # abre Antigravity na pasta atual
agy --version
```

### Primeiro setup do Antigravity

```
1. Abra Antigravity pelo Spotlight ou Finder > Applications
2. Sign in com sua conta Google (Gmail pessoal — Workspace não suportado no preview)
3. Aguarde inicialização (~2-3 min no primeiro launch)
4. Conceda permissões: File System Access, Network Connections
5. Em Settings > AI Model: selecione "Gemini 3.1 Pro" (ou Claude Sonnet se preferir)
6. Em Settings > Autonomy: escolha "Review-Driven" (recomendado — agente propõe, você aprova)
```

---

## 5. Instalar SpecKit na raiz do projeto

```bash
# Instalar o CLI do SpecKit via uv (método oficial mais atual)
uv tool install specify-cli --from git+https://github.com/github/spec-kit.git

# Verificar
specify --version

# Criar o projeto (inicializa estrutura SpecKit)
mkdir rinha-2026-c
cd rinha-2026-c
git init

# Inicializar SpecKit para Gemini CLI
specify init --agent gemini

# ou para Antigravity (que é compatível com Claude também):
specify init --agent claude
# Nota: o SpecKit da Antigravity funciona via .agent/ folder;
# para Claude Code / Antigravity, renomear .claude para .agent se necessário

# O que será criado:
# .specify/
#   memory/
#   scripts/
#   templates/
# .gemini/                (se --agent gemini)
#   agents/
#     speckit.constitution.md
#     speckit.specify.md
#     speckit.plan.md
#     speckit.tasks.md
#     speckit.implement.md
#     speckit.review.md
```

### Estrutura após `specify init`

```
rinha-2026-c/
├── .specify/
│   ├── memory/
│   │   └── constitution.md    ← gerado pelo speckit.constitution
│   ├── scripts/
│   └── templates/
├── .gemini/
│   └── agents/               ← slash commands do SpecKit
└── (seu código virá aqui)
```

---

## 6. Fluxo SDD de Ponta a Ponta

### Fase 1 — Constitution (regras imutáveis do projeto)

```bash
cd rinha-2026-c
gemini
```

Dentro do REPL do Gemini:
```
/speckit.constitution
```
Cole o prompt da seção 7.1 abaixo e pressione Enter. O agente gera `.specify/memory/constitution.md`.

### Fase 2 — Specify (o quê e por quê)

```
/speckit.specify
```
Cole o prompt da seção 7.2. Gera `.specify/specs/fraud-api.md`.

### Fase 3 — Plan (como, stack, arquitetura)

```
/speckit.plan
```
Cole o prompt da seção 7.3. Gera `.specify/plans/fraud-api-plan.md`.

### Fase 4 — Tasks (lista de tarefas ordenada)

```
/speckit.tasks
```
Gera `.specify/tasks/fraud-api-tasks.md` com tarefas granulares.

### Fase 5 — Implement (o agente escreve o código)

```
/speckit.implement
```
O agente executa todas as tarefas, criando os arquivos. Revise cada etapa quando o modo Review-Driven pedir aprovação.

**Dica:** Para tarefas grandes, divida em chamadas menores. Se o Gemini CLI travar, use o Antigravity (`agy .`) que tem Mission Control para monitorar agentes em paralelo.

---

## 7. Prompts para cada fase do SpecKit

### 7.1 — Constitution

```
Create a constitution for a high-performance C project targeting a fraud detection
competition (Rinha de Backend 2026). Rules:

- Language: C11 only (gcc or clang). Zero external runtime dependencies (only libc + POSIX + zlib).
- No dynamic memory allocation in the hot path (per-request). Use static pools and stack allocation.
- All structs must be cache-aligned. Use float32 (not double) for vector math.
- I/O model: epoll edge-triggered with SO_REUSEPORT. Never use blocking I/O in the worker path.
- Build: gcc -O2 -march=native -std=c11 -Wall -Wextra -Wpedantic -D_GNU_SOURCE.
- Code style: clang-format with 4-space indent, 100 char line limit, max 60 lines per function.
- Testing: Unity framework for unit tests. Minimum 80% function coverage on fraud/ and api/ modules.
- All public functions must have contract comments (params, return, preconditions).
- Docker: multi-stage Dockerfile (builder debian:12 + runtime debian:12-slim). Final image < 100 MB.
- Docker Compose: network mode bridge only. Total limits: 1.0 CPU, 350 MB RAM across all services.
- The system exposes port 9999 via a custom C load balancer (no nginx, no haproxy).
- Security: zero UBSan violations, zero ASan memory leaks in test builds.
```

### 7.2 — Specify

```
Build a fraud detection API for the Rinha de Backend 2026 competition. Full context:

SYSTEM:
- Three C binaries: fraud-api (worker), lb (load balancer), data-loader (init container).
- POSIX shared memory (/rinha_refstore) holds the 1M reference vectors. Workers mmap it read-only.
- The load balancer listens on port 9999, proxies to workers on ports 9001 and 9002.

API CONTRACT:
- GET /ready → 200 when dataset is loaded, 503 otherwise.
- POST /fraud-score → body: transaction JSON (see below). Response: {"approved": bool, "fraud_score": float}.

FRAUD DETECTION LOGIC:
- Vectorize the transaction into 14 float32 dimensions (0.0–1.0, except indices 5&6 = -1 when last_transaction is null).
- KNN search: K=5, euclidean distance, brute-force over 1M reference vectors.
- fraud_score = count_fraud_in_top5 / 5.0
- approved = fraud_score < 0.6

DATASET FILES (loaded at startup, never change during test):
- references.json.gz: 1M labeled vectors [{vector:[14 floats], label:"fraud"|"legit"}]
- mcc_risk.json: MCC code → float risk (default 0.5 for unknown)
- normalization.json: max constants for clamping

VECTOR DIMENSIONS (in order):
0: amount/max_amount
1: installments/max_installments
2: (amount/avg_amount)/amount_vs_avg_ratio
3: hour_of_day (UTC, 0-23 normalized to 0-1)
4: day_of_week (0=Mon, 6=Sun, normalized to 0-1)
5: minutes_since_last_tx/max_minutes (or -1.0 if no last_tx)
6: km_from_last_tx/max_km (or -1.0 if no last_tx)
7: km_from_home/max_km
8: tx_count_24h/max_tx_count_24h
9: is_online (0.0 or 1.0)
10: card_present (0.0 or 1.0)
11: unknown_merchant (1.0 if merchant.id NOT in known_merchants, else 0.0)
12: mcc_risk (from mcc_risk.json, default 0.5)
13: merchant_avg_amount/max_merchant_avg_amount

All clamped to [0.0,1.0] except sentinels at 5,6.
```

### 7.3 — Plan

```
/speckit.plan

Tech stack and architecture decisions:
- Language: C11 (gcc)
- Three binaries: fraud-api, lb, data-loader — compiled from same source tree via Makefile
- data-loader: reads resources/references.json.gz (zlib inflate), parses JSON, writes flat array of
  ref_vector_t structs to POSIX shm /rinha_refstore. Exits 0 when done.
- fraud-api: mmaps /rinha_refstore read-only. epoll + SO_REUSEPORT thread pool.
  Zero malloc per request. Buffer pool of conn_ctx_t per thread.
  JSON parser custom single-pass scanner (no deps). HTTP/1.1 keep-alive.
- lb: single-thread epoll proxy. Round-robin with atomic counter. Connection pool (32 conns per upstream).
  Health check passive: marks upstream DEAD on ECONNREFUSED, retries after 5s.
- KNN: brute-force O(N*14) scan. Linear array of 5 best candidates (no heap — K is tiny).
  Distance = euclidean squared (skip sqrt for comparison).
  Loop structured for GCC auto-vectorization (SIMD/AVX2).
- MCC lookup: linear scan of 10-entry static array (faster than hash for this size).
- Merchant lookup: FNV-1a 32-bit hash comparison (O(1), no strcmp).
- Shared memory layout: magic uint64 + count uint32 + ref_vector_t[100000] flat array (~6MB total).
- Docker: multi-stage. debian:12 builder, debian:12-slim runtime. All 3 binaries same image.
- docker-compose: data-loader (init), api1:9001 (0.4cpu/150MB), api2:9002 (0.4cpu/150MB),
  lb:9999 (0.2cpu/50MB). Total: 1.0cpu / 350MB.

Directory structure: src/api/, src/fraud/, src/lb/, src/loader/, src/common/, test/unit/, test/integration/
Build: single Makefile with targets: all, fraud-api, lb, data-loader, test, lint, docker
```

### 7.4 — Tasks (deixe o agente gerar, mas oriente se necessário)

```
/speckit.tasks

Break the implementation plan into ordered tasks. Each task must:
- Take at most 15 minutes of agent work
- Produce compilable, testable code
- Not depend on more than 1 future task

Priority order:
1. Build scaffolding (Makefile, directory structure, types.h)
2. data-loader: zlib inflate + JSON parser for references.json.gz
3. data-loader: POSIX shm write
4. fraud-api: shm reader + /ready endpoint
5. fraud/vectorize.c: all 14 dimensions + unit tests
6. fraud/knn.c: brute-force KNN K=5 + unit tests
7. api/json.c: POST /fraud-score JSON parser
8. api/http.c: epoll + SO_REUSEPORT + keep-alive
9. lb: epoll proxy + round-robin + connection pool
10. Dockerfile multi-stage
11. docker-compose.yml with resource limits
12. Integration test scripts (curl + expected values)
```

### 7.5 — Implement

```
/speckit.implement

Implement all tasks in order. For each task:
- Write the code file(s)
- Run: make [target] to verify it compiles with zero warnings
- Run unit tests if the task has them: make test
- Report any issue before proceeding to the next task
- Do not skip the compile check

Important constraints:
- NEVER use malloc/calloc in src/api/ or src/fraud/ (only in data-loader init path)
- ALWAYS use restrict on pointer params that don't alias
- ALWAYS check return values of syscalls (recv, send, mmap, shm_open, epoll_ctl)
- ALWAYS close file descriptors on every error path
- The KNN inner loop MUST be a simple float subtract/multiply/add loop (for SIMD auto-vec)
```

---

## 8. Testar no macOS — Estratégia Correta

### 8.1 O problema do macOS + Docker

O Docker Desktop no macOS roda dentro de uma VM Linux (Apple Hypervisor). Os limites de `cpus` e `memory` no `docker-compose.yml` são aplicados dentro da VM, mas a VM inteira tem acesso a todos os cores do Mac. Na prática:

- Os containers vão **ignorar efetivamente** os limites de CPU em workloads leves
- Os limites de **memória** são respeitados (o OOM killer funciona dentro da VM)
- Para um teste realista de CPU, você precisa de Linux real (VM local ou CI no Linux)

### 8.2 Testes que valem fazer no Mac

**Nível 1 — Corretude funcional (totalmente válido no Mac)**

```bash
# Subir o stack completo
docker compose up --build -d

# Aguardar ready
until curl -sf http://localhost:9999/ready; do sleep 1; done
echo "API pronta"

# Testar caso legítimo (fraud_score esperado: 0.0, approved: true)
curl -s -X POST http://localhost:9999/fraud-score \
  -H "Content-Type: application/json" \
  -d '{
    "id": "tx-1329056812",
    "transaction": {"amount": 41.12, "installments": 2, "requested_at": "2026-03-11T18:45:53Z"},
    "customer": {"avg_amount": 82.24, "tx_count_24h": 3, "known_merchants": ["MERC-003", "MERC-016"]},
    "merchant": {"id": "MERC-016", "mcc": "5411", "avg_amount": 60.25},
    "terminal": {"is_online": false, "card_present": true, "km_from_home": 29.23},
    "last_transaction": null
  }' | jq .

# Espera: {"approved":true,"fraud_score":0.0}

# Testar caso fraudulento (fraud_score esperado: 1.0, approved: false)
curl -s -X POST http://localhost:9999/fraud-score \
  -H "Content-Type: application/json" \
  -d '{
    "id": "tx-3330991687",
    "transaction": {"amount": 9505.97, "installments": 10, "requested_at": "2026-03-14T05:15:12Z"},
    "customer": {"avg_amount": 81.28, "tx_count_24h": 20, "known_merchants": ["MERC-008", "MERC-007"]},
    "merchant": {"id": "MERC-068", "mcc": "7802", "avg_amount": 54.86},
    "terminal": {"is_online": false, "card_present": true, "km_from_home": 952.27},
    "last_transaction": null
  }' | jq .

# Espera: {"approved":false,"fraud_score":1.0}
```

**Nível 2 — Teste de carga para latência (indicativo no Mac, não definitivo)**

```bash
# Instalar k6
brew install k6

# Criar script de carga rinha-test.js (ver seção 8.3)
k6 run rinha-test.js
```

**Nível 3 — Verificação de memory usage**

```bash
# Ver uso real de memória dos containers
docker stats --no-stream

# Ou monitorar continuamente durante o teste k6:
watch -n 1 'docker stats --no-stream'
```

### 8.3 Script k6 de carga (baseado no padrão da Rinha)

Crie `test/load/rinha-test.js`:

```javascript
import http from 'k6/http';
import { check, sleep } from 'k6';

// Payloads baseados nos exemplos do repositório
const payloads = [
  {
    "id": "tx-legit-001",
    "transaction": {"amount": 41.12, "installments": 2, "requested_at": "2026-03-11T18:45:53Z"},
    "customer": {"avg_amount": 82.24, "tx_count_24h": 3, "known_merchants": ["MERC-003", "MERC-016"]},
    "merchant": {"id": "MERC-016", "mcc": "5411", "avg_amount": 60.25},
    "terminal": {"is_online": false, "card_present": true, "km_from_home": 29.23},
    "last_transaction": null
  },
  {
    "id": "tx-fraud-001",
    "transaction": {"amount": 9505.97, "installments": 10, "requested_at": "2026-03-14T05:15:12Z"},
    "customer": {"avg_amount": 81.28, "tx_count_24h": 20, "known_merchants": ["MERC-008", "MERC-007"]},
    "merchant": {"id": "MERC-068", "mcc": "7802", "avg_amount": 54.86},
    "terminal": {"is_online": false, "card_present": true, "km_from_home": 952.27},
    "last_transaction": null
  }
];

export let options = {
  stages: [
    { duration: '10s', target: 10  },  // ramp up
    { duration: '30s', target: 50  },  // carga nominal
    { duration: '30s', target: 100 },  // pico
    { duration: '10s', target: 0   },  // ramp down
  ],
  thresholds: {
    http_req_duration: ['p(99)<100'],   // p99 < 1ms
    http_req_failed:   ['rate<0.01'],   // < 1% erros
  },
};

export default function () {
  const payload = payloads[Math.floor(Math.random() * payloads.length)];
  const res = http.post(
    'http://localhost:9999/fraud-score',
    JSON.stringify(payload),
    { headers: { 'Content-Type': 'application/json' } }
  );

  check(res, {
    'status is 200': (r) => r.status === 200,
    'has approved field': (r) => JSON.parse(r.body).approved !== undefined,
    'has fraud_score field': (r) => JSON.parse(r.body).fraud_score !== undefined,
    'fraud_score in [0,1]': (r) => {
      const score = JSON.parse(r.body).fraud_score;
      return score >= 0.0 && score <= 1.0;
    },
  });
}
```

```bash
k6 run test/load/rinha-test.js
```

### 8.4 Script de teste de corretude (smoke test completo)

Crie `test/integration/smoke.sh`:

```bash
#!/bin/bash
set -e

BASE="http://localhost:9999"
PASS=0
FAIL=0

assert_json() {
  local desc="$1"
  local field="$2"
  local expected="$3"
  local actual="$4"
  if [ "$actual" = "$expected" ]; then
    echo "✅ PASS: $desc"
    PASS=$((PASS+1))
  else
    echo "❌ FAIL: $desc — esperado '$expected', obtido '$actual'"
    FAIL=$((FAIL+1))
  fi
}

# Aguardar API
echo "Aguardando /ready..."
for i in $(seq 1 30); do
  if curl -sf "$BASE/ready" > /dev/null 2>&1; then
    echo "✅ API ready"
    break
  fi
  sleep 1
done

# Caso legítimo
RESP=$(curl -sf -X POST "$BASE/fraud-score" \
  -H "Content-Type: application/json" \
  -d '{"id":"tx-1329056812","transaction":{"amount":41.12,"installments":2,"requested_at":"2026-03-11T18:45:53Z"},"customer":{"avg_amount":82.24,"tx_count_24h":3,"known_merchants":["MERC-003","MERC-016"]},"merchant":{"id":"MERC-016","mcc":"5411","avg_amount":60.25},"terminal":{"is_online":false,"card_present":true,"km_from_home":29.23},"last_transaction":null}')

APPROVED=$(echo "$RESP" | python3 -c "import sys,json; print(str(json.load(sys.stdin)['approved']).lower())")
SCORE=$(echo "$RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['fraud_score'])")

assert_json "Caso legítimo: approved=true"   "approved" "true"  "$APPROVED"
assert_json "Caso legítimo: fraud_score=0.0" "score"    "0.0"   "$SCORE"

# Caso fraudulento
RESP2=$(curl -sf -X POST "$BASE/fraud-score" \
  -H "Content-Type: application/json" \
  -d '{"id":"tx-3330991687","transaction":{"amount":9505.97,"installments":10,"requested_at":"2026-03-14T05:15:12Z"},"customer":{"avg_amount":81.28,"tx_count_24h":20,"known_merchants":["MERC-008","MERC-007"]},"merchant":{"id":"MERC-068","mcc":"7802","avg_amount":54.86},"terminal":{"is_online":false,"card_present":true,"km_from_home":952.27},"last_transaction":null}')

APPROVED2=$(echo "$RESP2" | python3 -c "import sys,json; print(str(json.load(sys.stdin)['approved']).lower())")
SCORE2=$(echo "$RESP2" | python3 -c "import sys,json; print(json.load(sys.stdin)['fraud_score'])")

assert_json "Caso fraudulento: approved=false"  "approved" "false" "$APPROVED2"
assert_json "Caso fraudulento: fraud_score=1.0" "score"    "1.0"   "$SCORE2"

echo ""
echo "Resultado: $PASS passaram, $FAIL falharam"
[ $FAIL -eq 0 ] && exit 0 || exit 1
```

```bash
chmod +x test/integration/smoke.sh
docker compose up --build -d
./test/integration/smoke.sh
```

---

## 9. Simular Limites de CPU/Memória Localmente

O Mac bypassa os limites de CPU, mas você pode obter uma leitura mais realista das opções abaixo:

### Opção A — VM Linux local com Lima (mais prático, gratuito)

```bash
# Instalar Lima (VM Linux leve no Mac)
brew install lima

# Criar VM com limites iguais ao servidor da Rinha
limactl create --name rinha \
  --cpus 1 \
  --memory 1 \
  template://ubuntu

limactl start rinha

# Entrar na VM
limactl shell rinha

# Dentro da VM — instalar Docker
sudo apt-get update
sudo apt-get install -y docker.io docker-compose-v2
sudo usermod -aG docker $USER
newgrp docker

# Copiar projeto para a VM (ou clonar do git)
# git clone https://github.com/seu-usuario/rinha-2026-c .
# docker compose up --build -d
# k6 run rinha-test.js  (instalar k6 na VM também)
```

### Opção B — Colima (alternativa mais simples ao Lima)

```bash
brew install colima
colima start --cpu 1 --memory 1 --disk 20

# Docker já funciona com Colima como backend
docker compose up --build -d
```

### Opção C — GitHub Actions (testa no Linux real, gratuito)

Crie `.github/workflows/test.yml`:

```yaml
name: Rinha Test
on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Build e subir containers
        run: docker compose up --build -d

      - name: Aguardar /ready
        run: |
          for i in $(seq 1 30); do
            curl -sf http://localhost:9999/ready && break
            sleep 1
          done

      - name: Smoke test
        run: bash test/integration/smoke.sh

      - name: Instalar k6
        run: |
          sudo gpg -k
          sudo gpg --no-default-keyring --keyring /usr/share/keyrings/k6-archive-keyring.gpg \
            --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys C5AD17C747E3415A3642D57D77C6C491D6AC1D69
          echo "deb [signed-by=/usr/share/keyrings/k6-archive-keyring.gpg] https://dl.k6.io/deb stable main" \
            | sudo tee /etc/apt/sources.list.d/k6.list
          sudo apt-get update && sudo apt-get install -y k6

      - name: Teste de carga k6
        run: k6 run test/load/rinha-test.js

      - name: Stats finais
        run: docker stats --no-stream
```

### Opção D — Compilar nativamente no Mac para testes unitários

Para rodar os testes de unidade **sem Docker** (mais rápido no ciclo de dev):

```bash
# Compilar para macOS (somente testes de unidade, sem shm POSIX)
make test PLATFORM=macos

# O Makefile deve ter:
# ifeq ($(PLATFORM),macos)
#   CFLAGS += -D_MACOS_TEST
#   LDFLAGS = -lpthread   (sem -lrt no macOS)
# endif

# Os testes de vectorize e knn não dependem de epoll/shm,
# então compilam e rodam diretamente no Mac.
```

---

## 10. Checklist de Submissão

```bash
# 1. Build multiplataforma linux/amd64 (obrigatório para o servidor da Rinha)
docker buildx create --use
docker buildx build --platform linux/amd64 -t seu-usuario/rinha-2026:latest --push .

# 2. Verificar que a imagem funciona em linux/amd64
docker run --platform linux/amd64 --rm seu-usuario/rinha-2026:latest /usr/local/bin/fraud-api --version

# 3. Verificar limites no docker-compose.yml
# Soma de todos os serviços NÃO pode ultrapassar:
#   cpus: 1.0 total
#   memory: 350M total

# 4. Verificar que o repo tem:
# - docker-compose.yml na raiz do diretório de participante
# - Imagens públicas no Docker Hub (não imagens locais)
# - README.md com link para o código fonte
# - Modo de rede: bridge (não host, não privileged)

# 5. Teste final antes do PR
docker compose down -v
docker compose pull        # garante que as imagens são as publicadas
docker compose up -d
sleep 10
curl -sf http://localhost:9999/ready
./test/integration/smoke.sh

# 6. Abrir PR no repositório da Rinha
# Branch: submission
# Adicionar arquivo em participants/<seu-github>.json

# 7. Para rodar o teste oficial:
# Abrir uma issue com "rinha/test" no título
# A engine da Rinha executa automaticamente e comenta o resultado
```

---

## Resumo Visual do Fluxo

```
macOS Terminal
│
├─ brew install node gcc docker k6 wget
├─ uv tool install specify-cli            → SpecKit instalado
├─ brew install gemini-cli                → Gemini CLI instalado
├─ brew install --cask antigravity        → Antigravity IDE instalado
│
├─ mkdir rinha-2026-c && specify init --agent gemini
│
├─ gemini (REPL)
│   ├─ /speckit.constitution   → constitution.md
│   ├─ /speckit.specify        → spec.md
│   ├─ /speckit.plan           → plan.md
│   ├─ /speckit.tasks          → tasks.md
│   └─ /speckit.implement      → src/ Makefile Dockerfile docker-compose.yml
│
├─ docker compose up --build -d
├─ ./test/integration/smoke.sh            → corretude ✅
├─ k6 run test/load/rinha-test.js         → latência (indicativo)
├─ limactl shell rinha → k6 run ...       → latência realista (Linux)
│
└─ docker buildx build --platform linux/amd64 --push .
   → submissão no repo da Rinha
```
