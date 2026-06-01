FROM debian:12 AS builder
RUN apt-get update && apt-get install -y gcc make zlib1g-dev
COPY . /app
WORKDIR /app
RUN make all

FROM debian:12-slim
RUN apt-get update && apt-get install -y zlib1g curl && rm -rf /var/lib/apt/lists/*
WORKDIR /app
RUN mkdir -p /app/resources /app/data && chmod 777 /app/data
COPY --from=builder /app/bin/* /usr/local/bin/
COPY --from=builder /app/resources /app/resources
COPY start.sh /app/start.sh
RUN chmod +x /app/start.sh
# Sem ENTRYPOINT para permitir que o comando do compose funcione como antes
