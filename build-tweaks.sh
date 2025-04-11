#!/bin/bash

set -eux

export CMAKE_CXX_COMPILER_LAUNCHER=ccache

export CMAKE_INSTALL_PREFIX=/usr/local/stow/dcmtk-tweaks
export DCMTK_DEFAULT_DICT=builtin
export DCMTK_ENABLE_PRIVATE_TAGS=ON
export DCMTK_PORTABLE_LINUX_BINARIES=ON

ROOT=$(dirname "$(readlink -e "$0")")
case $(basename $0 .sh) in
    configure)
	cd $ROOT
	[ -f CMakeLists.txt ] || cmake ..
    ;;

    build)
	cd $ROOT/tweaks
	make -j$(nproc) all
    ;;

    install)
	cd $ROOT/tweaks
	sudo cmake -DCMAKE_INSTALL_COMPONENT=bin -P cmake_install.cmake
    ;;

    *)
	mkdir -p $ROOT/build
	cd $ROOT/build
	ln -fs ../$(basename $0) configure.sh
	ln -fs ../$(basename $0) build.sh
	ln -fs ../$(basename $0) install.sh
	
	./configure.sh
	./build.sh
	./install.sh
    ;;
esac
