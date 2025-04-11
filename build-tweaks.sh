#!/bin/bash

set -eux

export CMAKE_CXX_COMPILER_LAUNCHER=ccache

export CMAKE_INSTALL_PREFIX=/usr/local/stow/dcmtk-tweaks
export DCMTK_DEFAULT_DICT=builtin
export DCMTK_ENABLE_PRIVATE_TAGS=ON
export DCMTK_PORTABLE_LINUX_BINARIES=ON

mkdir -p build
cd build
cmake ..
cd tweaks
make -j$(nproc) all
sudo cmake -DCMAKE_INSTALL_COMPONENT=bin -P cmake_install.cmake
