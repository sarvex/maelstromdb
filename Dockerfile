FROM ubuntu:20.04 AS builder

# Prevent interactive tool from blocking package installations
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y build-essential cmake git pkg-config protobuf-compiler

WORKDIR /git
RUN git clone --recurse-submodules -b 3.19.x https://github.com/protocolbuffers/protobuf /git/protobuf
RUN cd /git/protobuf && \
    mkdir -p cmake/build && \
    cd cmake/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make install

RUN git clone --recurse-submodules -b v1.43.0 https://github.com/grpc/grpc /git/grpc
RUN cd /git/grpc && \
    mkdir -p cmake/build && \
    cd cmake/build && \
    cmake -DBUILD_DEPS=ON -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF ../.. && \
    make -j$(nproc) && \
    make install

RUN git clone --recurse-submodules -b v1.12.x https://github.com/google/googletest /git/googletest
RUN cd /git/googletest && \
    mkdir -p cmake/build && \
    cd cmake/build && \
    cmake ../.. && \
    make install

COPY . /usr/src
WORKDIR /usr/src

RUN cmake -S . -B build
RUN cmake --build build

FROM ubuntu:20.04 AS runtime

COPY --from=builder /usr/src/build/bin /usr/src/app
RUN ldconfig
WORKDIR /usr/src/app

