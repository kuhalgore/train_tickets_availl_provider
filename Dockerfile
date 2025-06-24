# ---------- Stage 1: Build ----------
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    g++ cmake git libboost-all-dev libssl-dev libcurl4-openssl-dev \
    autoconf automake libtool pkg-config

# Set working directory
WORKDIR /app

# Copy source code
COPY . .

# Build the app
RUN mkdir build && cd build && cmake .. && make

# ---------- Stage 2: Runtime ----------
FROM ubuntu:22.04

# Install runtime dependencies only
RUN apt-get update && apt-get install -y \
    libboost-system-dev libssl-dev libcurl4-openssl-dev && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy built binary and any required runtime files
COPY --from=builder /app/build/TrainTicketsAvailProvider ./TrainTicketsAvailProvider
COPY --from=builder /app/libs ./libs

# Set RPATH for shared libraries
ENV LD_LIBRARY_PATH=/app/libs

EXPOSE 18080
CMD ["./TrainTicketsAvailProvider"]