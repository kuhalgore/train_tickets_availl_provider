# ---------- Stage 1: Build ----------
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    curl \
    build-essential \
    cmake \
    git \
    libssl-dev \
    autoconf \
    automake \
    libtool \
    pkg-config \
    zlib1g-dev \
    libpsl-dev \
    ca-certificates \
    libboost-regex-dev && \
    rm -rf /var/lib/apt/lists/*

# Build libcurl from source
RUN git clone https://github.com/curl/curl.git /tmp/curl && \
    cd /tmp/curl && ./buildconf && ./configure --with-ssl && \
    make -j$(nproc) && make install && \
    rm -rf /tmp/curl

# Build Mailio from source
RUN git clone https://github.com/karastojko/mailio.git /tmp/mailio && \
    cd /tmp/mailio && mkdir build && cd build && \
    cmake .. && make -j$(nproc) && make install && \
    rm -rf /tmp/mailio

# Build your app
WORKDIR /app
COPY . .
RUN mkdir build && cd build && cmake .. && make

# ---------- Runtime ----------
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libssl-dev \
    zlib1g \
    libboost-regex-dev && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# Copy built libraries and app binary
COPY --from=builder /usr/local/lib /usr/local/lib
COPY --from=builder /usr/local/include /usr/local/include
COPY --from=builder /app/build/TrainTicketsAvailProvider /app/TrainTicketsAvailProvider

WORKDIR /app
ENV LD_LIBRARY_PATH=/usr/local/lib

EXPOSE 18080
CMD ["./TrainTicketsAvailProvider"]
