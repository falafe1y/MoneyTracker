#!/usr/bin/env bash
rm -rf build-ninja/
cmake -S . -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Debug -DVEXA_BUILD_TESTS=OFF -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build build-ninja -j3
