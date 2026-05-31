FROM debian:12 AS builder
RUN apt-get update && apt-get install -y gcc make zlib1g-dev
COPY . /app
WORKDIR /app
# O Makefile agora detecta a arquitetura automaticamente
RUN make all

FROM debian:12-slim
RUN apt-get update && apt-get install -y zlib1g curl && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=builder /app/bin/* /usr/local/bin/
# Copia a pasta resources (que a Engine da Rinha preenche antes do build)
COPY --from=builder /app/resources /app/resources
COPY start.sh /app/start.sh
RUN chmod +x /app/start.sh
EXPOSE 9999
