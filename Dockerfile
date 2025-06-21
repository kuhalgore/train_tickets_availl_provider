FROM debian:bullseye-slim

# Install dependencies
RUN apt-get update && apt-get install -y \
    g++ cmake curl libssl-dev libasio-dev \
    libboost-system-dev libboost-date-time-dev libboost-regex-dev \
    libcurl4-openssl-dev \
    && apt-get clean

# Set working directory
WORKDIR /app

# Copy entire project
COPY . .

# Build the application
RUN mkdir build && cd build && cmake .. && make

# Expose Crow's default port
EXPOSE 8080

# Run the binary
CMD ["./build/TrainTicketsAvailProvider"]