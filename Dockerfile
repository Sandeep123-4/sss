# Use Ubuntu 24.04 as base
FROM ubuntu:24.04

# Install build tools, ASIO, git, OpenSSL
RUN apt-get update && apt-get install -y \
    g++ \
    make \
    cmake \
    git \
    libasio-dev \
    libssl-dev \
    && apt-get clean

# Download WebSocket++ headers
RUN git clone https://github.com/zaphoyd/websocketpp.git /opt/websocketpp

# Set working directory
WORKDIR /app
COPY . /app

# Compile server
RUN g++ main.cpp -o server -std=c++17 -pthread -I/opt/websocketpp

# Expose port (Render sets $PORT automatically)
EXPOSE 9002

# Start server
CMD ["./server"]
