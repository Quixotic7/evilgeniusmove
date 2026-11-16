# The Move's cross compiler: the same base and toolchain as Schwung's own build image.
FROM debian:bookworm
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-aarch64-linux-gnu libc6-dev-arm64-cross binutils-aarch64-linux-gnu make file \
    && rm -rf /var/lib/apt/lists/*
