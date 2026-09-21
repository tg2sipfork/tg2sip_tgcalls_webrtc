#!/bin/bash

set -e

export LANG=ru_RU.UTF-8
export LANGUAGE=ru_RU:ru
export LC_ALL=ru_RU.UTF-8

export CC=clang
export CXX=clang++

echo "==> Configuring project..."

cmake \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DCMAKE_BUILD_TYPE=Release ..

echo "==> Building project..."

cmake --build . -j$(nproc)

echo "==> Build finished successfully!"
