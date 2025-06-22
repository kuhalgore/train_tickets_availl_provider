FROM ubuntu:22.04
   
RUN apt-get update && apt-get install -y \
    g++ cmake git libboost-all-dev libssl-dev libcurl4-openssl-dev \
    autoconf automake libtool pkg-config

RUN git clone https://github.com/dhoerl/htmlcxx.git /tmp/htmlcxx && \
    cd /tmp/htmlcxx && \
    autoreconf -i && \
    ./configure && \
    make && make install && \
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