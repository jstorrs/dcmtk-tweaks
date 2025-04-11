#!/bin/bash

set -eux

export CMAKE_CXX_COMPILER_LAUNCHER=ccache

export CMAKE_INSTALL_PREFIX=/usr/local/stow/dcmtk-tweaks
export DCMTK_DEFAULT_DICT=builtin
export DCMTK_ENABLE_PRIVATE_TAGS=ON
export DCMTK_PORTABLE_LINUX_BINARIES=ON

ROOT=$(dirname "$(readlink -e "$0")")
BUILD_DIR=$ROOT/build

case $(basename $0 .sh) in
    configure)
	cd $BUILD_DIR
	[ -f CMakeLists.txt ] \
	    || cmake \
		   -D CMAKE_INSTALL_PREFIX=/usr/local/stow/dcmtk-tweaks \
		   -D DCMTK_DEFAULT_DICT=builtin \
		   -D DCMTK_ENABLE_PRIVATE_TAGS=ON \
		   -D DCMTK_PORTABLE_LINUX_BINARIES=ON \
		   ..
    ;;

    build)
	cd $BUILD_DIR/tweaks
	make -j$(nproc) all
    ;;

    install)
	cd $BUILD_DIR/tweaks
	sudo cmake -DCMAKE_INSTALL_COMPONENT=bin -P cmake_install.cmake
    ;;

    *)
	mkdir -p $BUILD_DIR
	cd $BUILD_DIR
	ln -fs ../$(basename $0) configure.sh
	ln -fs ../$(basename $0) build.sh
	ln -fs ../$(basename $0) install.sh
	
	./configure.sh
	./build.sh
	./install.sh
    ;;
esac
