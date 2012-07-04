# -*- mode: gnumakefile; tab-width: 8; indent-tabs-mode: t; -*-
# vim: tabstop=4
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
BUILD_TYPE:=Release

ifndef BUILD_CMAKE_ARGS
	BUILD_CMAKE_ARGS:=
endif

ifndef BUILD_DIR
	BUILD_DIR:=$(shell dirname $(BLENDER_DIR))/build/$(OS_NCASE)
endif


# -----------------------------------------------------------------------------
# additional targets for the build configuration

# support 'make debug'
ifneq "$(findstring debug, $(MAKECMDGOALS))" ""
	BUILD_DIR:=$(BUILD_DIR)_debug
	BUILD_TYPE:=Debug
endif
ifneq "$(findstring lite, $(MAKECMDGOALS))" ""
	BUILD_DIR:=$(BUILD_DIR)_lite
	BUILD_CMAKE_ARGS:=$(BUILD_CMAKE_ARGS) -C$(BLENDER_DIR)/build_files/cmake/config/blender_lite.cmake
endif
ifneq "$(findstring headless, $(MAKECMDGOALS))" ""
	BUILD_DIR:=$(BUILD_DIR)_bpy
	BUILD_CMAKE_ARGS:=$(BUILD_CMAKE_ARGS) -C$(BLENDER_DIR)/build_files/cmake/config/blender_headless.cmake
endif
ifneq "$(findstring bpy, $(MAKECMDGOALS))" ""
	BUILD_DIR:=$(BUILD_DIR)_bpy
	BUILD_CMAKE_ARGS:=$(BUILD_CMAKE_ARGS) -C$(BLENDER_DIR)/build_files/cmake/config/bpy_module.cmake
endif


# -----------------------------------------------------------------------------
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


# -----------------------------------------------------------------------------
# Macro for configuring cmake

CMAKE_CONFIG = cmake $(BUILD_CMAKE_ARGS) \
                     -H$(BLENDER_DIR) \
                     -B$(BUILD_DIR) \
                     -DCMAKE_BUILD_TYPE:STRING=$(BUILD_TYPE)


# -----------------------------------------------------------------------------
# Tool for 'make config'

# X11 spesific
ifdef DISPLAY
	CMAKE_CONFIG_TOOL = cmake-gui
else 
	CMAKE_CONFIG_TOOL = ccmake
endif


# -----------------------------------------------------------------------------
# Build Blender
all:
	@echo
	@echo Configuring Blender ...

	# if test ! -f $(BUILD_DIR)/CMakeCache.txt ; then \
	# 	$(CMAKE_CONFIG); \
	# fi
	
	# do this always incase of failed initial build, could be smarter here...
	$(CMAKE_CONFIG)

	@echo
	@echo Building Blender ...
	$(MAKE) -C $(BUILD_DIR) -s -j $(NPROCS) install
	@echo
	@echo edit build configuration with: "$(BUILD_DIR)/CMakeCache.txt" run make again to rebuild.
	@echo blender installed, run from: "$(BUILD_DIR)/bin/blender"
	@echo

debug: all
lite: all
headless: all
bpy: all


# -----------------------------------------------------------------------------
# Configuration (save some cd'ing around)
config:
	$(CMAKE_CONFIG_TOOL) $(BUILD_DIR)


# -----------------------------------------------------------------------------
# Help for build targets
help:
	@echo ""
	@echo "Convenience targets provided for building blender, (multiple at once can be used)"
	@echo "  * debug     - build a debug binary"
	@echo "  * lite      - disable non essential features for a smaller binary and faster build"
	@echo "  * headless  - build without an interface (renderfarm or server automation)"
	@echo "  * bpy       - build as a python module which can be loaded from python directly"
	@echo ""
	@echo "  * config    - run cmake configuration tool to set build options"
	@echo ""
	@echo "  Note, passing the argument 'BUILD_DIR=path' when calling make will override the default build dir."
	@echo "  Note, passing the argument 'BUILD_CMAKE_ARGS=args' lets you add cmake arguments."
	@echo ""
	@echo ""
	@echo "Project Files for IDE's"
	@echo "  * project_qtcreator - QtCreator Project Files"
	@echo "  * project_netbeans  - NetBeans Project Files"
	@echo "  * project_eclipse   - Eclipse CDT4 Project Files"
	@echo ""
	@echo "Package Targets"
	@echo "  * package_debian  - build a debian package"
	@echo "  * package_pacman  - build an arch linux pacmanpackage"
	@echo "  * package_archive - build an archive package"
	@echo ""
	@echo "Testing Targets (not associated with building blender)"
	@echo "  * test            - run ctest, currently tests import/export, operator execution and that python modules load"
	@echo "  * test_cmake      - runs our own cmake file checker which detects errors in the cmake file list definitions"
	@echo "  * test_pep8       - checks all python script are pep8 which are tagged to use the stricter formatting"
	@echo "  * test_deprecated - checks for deprecation tags in our code which may need to be removed"
	@echo "  * test_style      - checks C/C++ conforms with blenders style guide: http://wiki.blender.org/index.php/Dev:Doc/CodeStyle"
	@echo ""
	@echo "Static Source Code Checking (not associated with building blender)"
	@echo "  * check_cppcheck    - run blender source through cppcheck (C & C++)"
	@echo "  * check_splint      - run blenders source through splint (C only)"
	@echo "  * check_sparse      - run blenders source through sparse (C only)"
	@echo "  * check_spelling_c  - check for spelling errors (C/C++ only)"
	@echo "  * check_spelling_py - check for spelling errors (Python only)"
	@echo ""
	@echo "Documentation Targets (not associated with building blender)"
	@echo "  * doc_py   - generate sphinx python api docs"
	@echo "  * doc_doxy - generate doxygen C/C++ docs"
	@echo "  * doc_dna  - generate blender file format reference"
	@echo "  * doc_man  - generate manpage"
	@echo ""

# -----------------------------------------------------------------------------
# Packages
#
package_debian:
	cd build_files/package_spec ; DEB_BUILD_OPTIONS="parallel=$(NPROCS)" sh ./build_debian.sh

package_pacman:
	cd build_files/package_spec/pacman ; MAKEFLAGS="-j$(NPROCS)" makepkg --asroot

package_archive:
	make -C $(BUILD_DIR) -s package_archive
	@echo archive in "$(BUILD_DIR)/release"


# -----------------------------------------------------------------------------
# Tests
#
test:
	cd $(BUILD_DIR) ; ctest . --output-on-failure

# run pep8 check check on scripts we distribute.
test_pep8:
	python3.2 source/tests/pep8.py > test_pep8.log 2>&1
	@echo "written: test_pep8.log"

# run some checks on our cmakefiles.
test_cmake:
	python3.2 build_files/cmake/cmake_consistency_check.py > test_cmake_consistency.log 2>&1
	@echo "written: test_cmake_consistency.log"

# run deprecation tests, see if we have anything to remove.
test_deprecated:
	python3.2 source/tests/check_deprecated.py

test_style:
	# run our own checks on C/C++ style
	PYTHONIOENCODING=utf_8 python3.2 $(BLENDER_DIR)/source/tools/check_style_c.py $(BLENDER_DIR)/source/blender $(BLENDER_DIR)/source/creator --no-length-check

# -----------------------------------------------------------------------------
# Project Files
#

project_qtcreator:
	python3.2 build_files/cmake/cmake_qtcreator_project.py $(BUILD_DIR)

project_netbeans:
	python3.2 build_files/cmake/cmake_netbeans_project.py $(BUILD_DIR)

project_eclipse:
	cmake -G"Eclipse CDT4 - Unix Makefiles" -H$(BLENDER_DIR) -B$(BUILD_DIR)


# -----------------------------------------------------------------------------
# Static Checking
#

check_cppcheck:
	$(CMAKE_CONFIG)
	cd $(BUILD_DIR) ; python3.2 $(BLENDER_DIR)/build_files/cmake/cmake_static_check_cppcheck.py

check_splint:
	$(CMAKE_CONFIG)
	cd $(BUILD_DIR) ; python3.2 $(BLENDER_DIR)/build_files/cmake/cmake_static_check_splint.py

check_sparse:
	$(CMAKE_CONFIG)
	cd $(BUILD_DIR) ; python3.2 $(BLENDER_DIR)/build_files/cmake/cmake_static_check_sparse.py

check_spelling_py:
	cd $(BUILD_DIR) ; PYTHONIOENCODING=utf_8 python3.2 $(BLENDER_DIR)/source/tools/spell_check_source.py $(BLENDER_DIR)/release/scripts

check_spelling_c:
	cd $(BUILD_DIR) ; PYTHONIOENCODING=utf_8 python3.2 $(BLENDER_DIR)/source/tools/spell_check_source.py $(BLENDER_DIR)/source

# -----------------------------------------------------------------------------
# Documentation
#

# Simple version of ./doc/python_api/sphinx_doc_gen.sh with no PDF generation.
doc_py:
	$(BUILD_DIR)/bin/blender --background -noaudio --factory-startup --python doc/python_api/sphinx_doc_gen.py
	cd doc/python_api ; sphinx-build -b html sphinx-in sphinx-out
	@echo "docs written into: '$(BLENDER_DIR)/doc/python_api/sphinx-out/contents.html'"

doc_doxy:
	cd doc/doxygen; doxygen Doxyfile
	@echo "docs written into: '$(BLENDER_DIR)/doc/doxygen/html/index.html'"

doc_dna:
	$(BUILD_DIR)/bin/blender --background -noaudio --factory-startup --python doc/blender_file_format/BlendFileDnaExporter_25.py
	@echo "docs written into: '$(BLENDER_DIR)/doc/blender_file_format/dna.html'"

doc_man:
	python3.2 doc/manpage/blender.1.py $(BUILD_DIR)/bin/blender


clean:
	$(MAKE) -C $(BUILD_DIR) clean

.PHONY: all
