# Dockerfile
FROM ubuntu:24.04

# Install dependencies
RUN apt-get update && \
    apt-get install -y g++ make cmake libasio-dev git && \
    apt-get clean

# Copy project
WORKDIR /app
COPY . /app

# Build server
RUN g++ server.cpp -o server -pthread

# Expose Render port
EXPOSE 9002

# Start server
CMD ["./server"]
