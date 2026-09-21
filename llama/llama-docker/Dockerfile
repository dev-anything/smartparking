FROM arm64v8/ubuntu:22.04

RUN apt update && apt install -y \
    build-essential cmake git \
    libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

RUN git clone https://github.com/ggerganov/llama.cpp /llama.cpp
WORKDIR /llama.cpp

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build --config Release -j$(nproc)

EXPOSE 10001
ENTRYPOINT ["./build/bin/llama-server"]
