# Wind Waker HD for the PlayStation 5 - the builder kit's container: a Linux with what
# build-title.sh needs (LLVM 22, Python 3, a host C++ compiler) and the kit itself.
#
#   docker build -t wwhd-ps5-kit .
#   docker run --rm -v /folder/with/your/dump:/dump:ro -v "$PWD/out":/out wwhd-ps5-kit /dump/game.wux /out
#
# make-title.sh does both. Nothing of the game is in the image: the dump is read
# from /dump when the container runs, and what it makes goes to /out.
#
# SPDX-License-Identifier: MIT
FROM ubuntu:24.04
RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        ca-certificates wget python3 g++ libzstd-dev zlib1g-dev \
 && wget -qO /etc/apt/trusted.gpg.d/apt.llvm.org.asc https://apt.llvm.org/llvm-snapshot.gpg.key \
 && echo "deb https://apt.llvm.org/noble/ llvm-toolchain-noble-22 main" > /etc/apt/sources.list.d/llvm.list \
 && apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends clang-22 lld-22 llvm-22 \
 && rm -rf /var/lib/apt/lists/*
COPY . /kit
RUN bash /kit/build-title.sh --tools
WORKDIR /out
ENTRYPOINT ["bash", "/kit/build-title.sh"]
