FROM ubuntu:22.04

# Install standalone Asio
RUN git clone https://github.com/chriskohlhoff/asio.git /tmp/asio && \
    cp -r /tmp/asio/asio/include/asio /usr/local/include/

# Install dependencies
RUN apt-get update && apt-get install -y \
    g++ cmake git libboost-all-dev libssl-dev libcurl4-openssl-dev \
    autoconf automake libtool pkg-config

# Install htmlcxx (CMake-based fork)
RUN git clone https://github.com/pcoramasionwu/htmlcxx.git /tmp/htmlcxx && \
    cd /tmp/htmlcxx && \
    mkdir build && cd build && \
    cmake .. && make && make install && \
    ldconfig

# Set working directory
WORKDIR /app

# Copy source code
COPY . .

# Build the app
RUN mkdir build && cd build && cmake .. && make

# Expose the port
EXPOSE 18080

# Run the app
CMD ["./build/TrainTicketsAvailProvider"]