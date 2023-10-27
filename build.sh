#!/bin/sh

set -e

conan install . --output-folder=build-gcc11 --build=missing -c tools.system.package_manager:mode=install -c tools.system.package_manager:sudo=True
cd build-gcc11 || exit
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build .