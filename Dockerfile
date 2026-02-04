FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    g++ \
    git \
    cmake \
    libssl-dev \
    libasio-dev

# Download websocketpp headers
RUN git clone https://github.com/zaphoyd/websocketpp.git /opt/websocketpp

WORKDIR /app
COPY . /app

RUN g++ server.cpp -o server -std=c++17 -pthread -I/opt/websocketpp

CMD ["./server"]
