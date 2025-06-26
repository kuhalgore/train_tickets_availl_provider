# ---------- Stage 1: Build ----------
FROM ubuntu:22.04 AS builder
ENV DEBIAN_FRONTEND=noninteractive

# Install necessary packages
RUN apt-get update && apt-get install -y --no-install-recommends \
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
    libboost-system-dev \
    libboost-thread-dev \
    libboost-regex-dev \
    libboost-test-dev && \
    rm -rf /var/lib/apt/lists/*  # Clean up package lists

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

# ---------- Stage 2: Runtime ----------
FROM ubuntu:22.04

# Install only runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl-dev \
    zlib1g \
    libpsl-dev \
    libboost-system-dev \
    libboost-thread-dev \
    libboost-regex-dev \
    libboost-test-dev && \
    apt-get clean && rm -rf /var/lib/apt/lists/*  # Clean up package lists
    
RUN apt-get update && apt-get install -y ca-certificates


# Copy necessary files from the builder stage
COPY --from=builder /usr/local/lib /usr/local/lib
COPY --from=builder /usr/local/include /usr/local/include
COPY --from=builder /app/build/TrainTicketsAvailProvider /app/TrainTicketsAvailProvider

# Set working directory and environment variables
WORKDIR /app
ENV LD_LIBRARY_PATH=/usr/local/lib
EXPOSE 18080

# Command to run the application
CMD ["./TrainTicketsAvailProvider"]