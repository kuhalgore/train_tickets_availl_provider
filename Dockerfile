# ---------- Stage 1: Build ----------
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    g++ cmake git libssl-dev libboost-all-dev \
    autoconf automake libtool pkg-config make zlib1g-dev

# Build libcurl from source
WORKDIR /tmp
RUN git clone https://github.com/curl/curl.git && cd curl && \
    ./buildconf && ./configure --with-ssl && make -j$(nproc) && make install

# Build Mailio from source
RUN git clone https://github.com/karastojko/mailio.git && \
    cd mailio && mkdir build && cd build && \
    cmake .. && make -j$(nproc) && make install

# Build your app
WORKDIR /app
COPY . .
RUN mkdir build && cd build && cmake .. && make

# ---------- Stage 2: Runtime ----------
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl-dev libboost-system-dev zlib1g && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# Copy built libraries and binary
COPY --from=builder /usr/local/lib /usr/local/lib
COPY --from=builder /usr/local/include /usr/local/include
COPY --from=builder /app/build/TrainTicketsAvailProvider /app/TrainTicketsAvailProvider

WORKDIR /app
ENV LD_LIBRARY_PATH=/usr/local/lib

EXPOSE 18080
CMD ["./TrainTicketsAvailProvider"]
