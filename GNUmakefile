# -*- mode: gnumakefile; tab-width: 8; indent-tabs-mode: t; -*-
# vim: tabstop=8
# $Id$
#
# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# This Makefile does an out-of-source CMake build in ../build/`OS`_`CPU`
# eg:
#   ../build/Linux_i386
# This is for users who like to configure & build blender with a single command.


# System Vars
OS:=$(shell uname -s)
OS_NCASE:=$(shell uname -s | tr '[A-Z]' '[a-z]')
# CPU:=$(shell uname -m)  # UNUSED


# Source and Build DIR's
BLENDER_DIR:=$(shell pwd -P)
BUILD_DIR:=$(shell dirname $(BLENDER_DIR))/build/$(OS_NCASE)


# support 'make debug'
ifneq "$(findstring debug, $(MAKECMDGOALS))" ""
	BUILD_DIR:=$(BUILD_DIR)_debug
	BUILD_TYPE:=Debug
else
	BUILD_TYPE:=Release
endif


# Get the number of cores for threaded build
NPROCS:=1
ifeq ($(OS), Linux)
	NPROCS:=$(shell grep -c ^processor /proc/cpuinfo)
endif
ifeq ($(OS), Darwin)
	NPROCS:=$(shell sysctl -a | grep "hw.ncpu " | cut -d" " -f3)
endif
ifeq ($(OS), FreeBSD)
	NPROCS:=$(shell sysctl -a | grep "hw.ncpu " | cut -d" " -f3 )
endif
ifeq ($(OS), NetBSD)
	NPROCS:=$(shell sysctl -a | grep "hw.ncpu " | cut -d" " -f3 )
endif


# Build Blender
all:
	@echo
	@echo Configuring Blender ...

	if test ! -f $(BUILD_DIR)/CMakeCache.txt ; then \
		cmake -H$(BLENDER_DIR) -B$(BUILD_DIR) -DCMAKE_BUILD_TYPE:STRING=$(BUILD_TYPE) ; \
	fi

	@echo
	@echo Building Blender ...
	make -C $(BUILD_DIR) -s -j $(NPROCS) install
	@echo
	@echo edit build configuration with: "$(BUILD_DIR)/CMakeCache.txt" run make again to rebuild.
	@echo blender installed, run from: "$(BUILD_DIR)/bin/blender"
	@echo

debug: all
	# pass

# package types
package_debian:
	cd build_files/package_spec ; DEB_BUILD_OPTIONS="parallel=$(NPROCS)" sh ./build_debian.sh

package_pacman:
	cd build_files/package_spec/pacman ; MAKEFLAGS="-j$(NPROCS)" makepkg --asroot

package_archive:
	make -C $(BUILD_DIR) -s package_archive
	@echo archive in "$(BUILD_DIR)/release"

# forward build targets
test:
	cd $(BUILD_DIR) ; ctest . --output-on-failure

# run pep8 check check on scripts we distribute.
test_pep8:
	python3 source/tests/pep8.py > test_pep8.log 2>&1
	@echo "written: test_pep8.log"

# run some checks on our cmakefiles.
test_cmake:
	python3 build_files/cmake/cmake_consistency_check.py > test_cmake_consistency.log 2>&1
	@echo "written: test_cmake_consistency.log"

# run deprecation tests, see if we have anything to remove.
test_deprecated:
	python3 source/tests/check_deprecated.py

clean:
	make -C $(BUILD_DIR) clean

.PHONY: all
