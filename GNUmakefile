# -*- mode: gnumakefile; tab-width: 4; indent-tabs-mode: t; -*-
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

# This Makefile does an out-of-source CMake build in ../build_`OS`_`CPU`
# eg:
#   ../build_linux_i386
# This is for users who like to configure & build blender with a single command.

define HELP_TEXT

Convenience Targets
   Provided for building Blender, (multiple at once can be used).

   * debug:         Build a debug binary.
   * full:          Enable all supported dependencies & options.
   * lite:          Disable non essential features for a smaller binary and faster build.
   * release        Complete build with all options enabled including CUDA and Optix, matching the releases on blender.org
   * headless:      Build without an interface (renderfarm or server automation).
   * cycles:        Build Cycles standalone only, without Blender.
   * bpy:           Build as a python module which can be loaded from python directly.
   * deps:          Build library dependencies (intended only for platform maintainers).

   * developer:     Enable faster builds, error checking and tests, recommended for developers.
   * config:        Run cmake configuration tool to set build options.
   * ninja:         Use ninja build tool for faster builds.

   Note: passing the argument 'BUILD_DIR=path' when calling make will override the default build dir.
   Note: passing the argument 'BUILD_CMAKE_ARGS=args' lets you add cmake arguments.


Project Files
   Generate project files for development environments.

   * project_qtcreator:     QtCreator Project Files.
   * project_netbeans:      NetBeans Project Files.
   * project_eclipse:       Eclipse CDT4 Project Files.

Package Targets

   * package_debian:    Build a debian package.
   * package_pacman:    Build an arch linux pacman package.
   * package_archive:	Build an archive package.

Testing Targets
   Not associated with building Blender.

   * test:
     Run automated tests with ctest.
   * test_cmake:
     Runs our own cmake file checker
     which detects errors in the cmake file list definitions
   * test_pep8:
     Checks all python script are pep8
     which are tagged to use the stricter formatting
   * test_deprecated:
     Checks for deprecation tags in our code which may need to be removed

Static Source Code Checking
   Not associated with building Blender.

   * check_cppcheck:        Run blender source through cppcheck (C & C++).
   * check_clang_array:     Run blender source through clang array checking script (C & C++).
   * check_splint:          Run blenders source through splint (C only).
   * check_sparse:          Run blenders source through sparse (C only).
   * check_smatch:          Run blenders source through smatch (C only).
   * check_spelling_c:      Check for spelling errors (C/C++ only).
   * check_spelling_c_qtc:  Same as check_spelling_c but outputs QtCreator tasks format.
   * check_spelling_osl:    Check for spelling errors (OSL only).
   * check_spelling_py:     Check for spelling errors (Python only).
   * check_descriptions:    Check for duplicate/invalid descriptions.

Utilities
   Not associated with building Blender.

   * icons:
     Updates PNG icons from SVG files.

     Optionally pass in variables: 'BLENDER_BIN', 'INKSCAPE_BIN'
     otherwise default paths are used.

     Example
        make icons INKSCAPE_BIN=/path/to/inkscape

   * icons_geom:
     Updates Geometry icons from BLEND file.

     Optionally pass in variable: 'BLENDER_BIN'
     otherwise default paths are used.

     Example
        make icons_geom BLENDER_BIN=/path/to/blender

   * source_archive:
     Create a compressed archive of the source code.

   * update:
     updates git and all submodules

   * format
     Format source code using clang (uses PATHS if passed in). For example::

        make format PATHS="source/blender/blenlib source/blender/blenkernel"

Environment Variables

   * BUILD_CMAKE_ARGS:      Arguments passed to CMake.
   * BUILD_DIR:             Override default build path.
   * PYTHON:                Use this for the Python command (used for checking tools).
   * NPROCS:                Number of processes to use building (auto-detect when omitted).

Documentation Targets
   Not associated with building Blender.

   * doc_py:        Generate sphinx python api docs.
   * doc_doxy:      Generate doxygen C/C++ docs.
   * doc_dna:       Generate blender file format reference.
   * doc_man:       Generate manpage.

Information

   * help:              This help message.
   * help_features:     Show a list of optional features when building.

endef
# HELP_TEXT (end)

# This makefile is not meant for Windows
ifeq ($(OS),Windows_NT)
    $(error On Windows, use "cmd //c make.bat" instead of "make")
endif

# System Vars
OS:=$(shell uname -s)
OS_NCASE:=$(shell uname -s | tr '[A-Z]' '[a-z]')
CPU:=$(shell uname -m)


# Source and Build DIR's
BLENDER_DIR:=$(shell pwd -P)
BUILD_TYPE:=Release

# CMake arguments, assigned to local variable to make it mutable.
CMAKE_CONFIG_ARGS := $(BUILD_CMAKE_ARGS)

ifndef BUILD_DIR
	BUILD_DIR:=$(shell dirname "$(BLENDER_DIR)")/build_$(OS_NCASE)
endif

# Dependencies DIR's
DEPS_SOURCE_DIR:=$(BLENDER_DIR)/build_files/build_environment

ifndef DEPS_BUILD_DIR
	DEPS_BUILD_DIR:=$(BUILD_DIR)/deps
endif

ifndef DEPS_INSTALL_DIR
	DEPS_INSTALL_DIR:=$(shell dirname "$(BLENDER_DIR)")/lib/$(OS_NCASE)

	ifneq ($(OS_NCASE),darwin)
		# Add processor type to directory name
		DEPS_INSTALL_DIR:=$(DEPS_INSTALL_DIR)_$(CPU)
	endif
endif

# Allow to use alternative binary (pypy3, etc)
ifndef PYTHON
	PYTHON:=python3
endif

# For macOS python3 is not installed by default, so fallback to python binary
# in libraries, or python 2 for running make update to get it.
ifeq ($(OS_NCASE),darwin)
	ifeq (, $(shell command -v $(PYTHON)))
		PYTHON:=../lib/darwin/python/bin/python3.7m
		ifeq (, $(shell command -v $(PYTHON)))
			PYTHON:=python
		endif
	endif
endif

# -----------------------------------------------------------------------------
# additional targets for the build configuration

# support 'make debug'
ifneq "$(findstring debug, $(MAKECMDGOALS))" ""
	BUILD_DIR:=$(BUILD_DIR)_debug
	BUILD_TYPE:=Debug
endif
ifneq "$(findstring full, $(MAKECMDGOALS))" ""
	BUILD_DIR:=$(BUILD_DIR)_full
	CMAKE_CONFIG_ARGS:=-C"$(BLENDER_DIR)/build_files/cmake/config/blender_full.cmake" $(CMAKE_CONFIG_ARGS)
endif
ifneq "$(findstring lite, $(MAKECMDGOALS))" ""
	BUILD_DIR:=$(BUILD_DIR)_lite
	CMAKE_CONFIG_ARGS:=-C"$(BLENDER_DIR)/build_files/cmake/config/blender_lite.cmake" $(CMAKE_CONFIG_ARGS)
endif
ifneq "$(findstring release, $(MAKECMDGOALS))" ""
	BUILD_DIR:=$(BUILD_DIR)_release
	CMAKE_CONFIG_ARGS:=-C"$(BLENDER_DIR)/build_files/cmake/config/blender_release.cmake" $(CMAKE_CONFIG_ARGS)
endif
ifneq "$(findstring cycles, $(MAKECMDGOALS))" ""
	BUILD_DIR:=$(BUILD_DIR)_cycles
	CMAKE_CONFIG_ARGS:=-C"$(BLENDER_DIR)/build_files/cmake/config/cycles_standalone.cmake" $(CMAKE_CONFIG_ARGS)
endif
ifneq "$(findstring headless, $(MAKECMDGOALS))" ""
	BUILD_DIR:=$(BUILD_DIR)_headless
	CMAKE_CONFIG_ARGS:=-C"$(BLENDER_DIR)/build_files/cmake/config/blender_headless.cmake" $(CMAKE_CONFIG_ARGS)
endif
ifneq "$(findstring bpy, $(MAKECMDGOALS))" ""
	BUILD_DIR:=$(BUILD_DIR)_bpy
	CMAKE_CONFIG_ARGS:=-C"$(BLENDER_DIR)/build_files/cmake/config/bpy_module.cmake" $(CMAKE_CONFIG_ARGS)
endif

ifneq "$(findstring developer, $(MAKECMDGOALS))" ""
	CMAKE_CONFIG_ARGS:=-C"$(BLENDER_DIR)/build_files/cmake/config/blender_developer.cmake" $(CMAKE_CONFIG_ARGS)
endif

# -----------------------------------------------------------------------------
# build tool

ifneq "$(findstring ninja, $(MAKECMDGOALS))" ""
	CMAKE_CONFIG_ARGS:=$(CMAKE_CONFIG_ARGS) -G Ninja
	BUILD_COMMAND:=ninja
	DEPS_BUILD_COMMAND:=ninja
else
	ifneq ("$(wildcard $(BUILD_DIR)/build.ninja)","")
		BUILD_COMMAND:=ninja
	else
		BUILD_COMMAND:=make -s
	endif

	ifneq ("$(wildcard $(DEPS_BUILD_DIR)/build.ninja)","")
		DEPS_BUILD_COMMAND:=ninja
	else
		DEPS_BUILD_COMMAND:=make -s
	endif
endif

# -----------------------------------------------------------------------------
# Blender binary path

# Allow passing in own BLENDER_BIN so developers who don't
# use the default build path can still use utility helpers.
ifeq ($(OS), Darwin)
	BLENDER_BIN?="$(BUILD_DIR)/bin/Blender.app/Contents/MacOS/Blender"
else
	BLENDER_BIN?="$(BUILD_DIR)/bin/blender"
endif


# -----------------------------------------------------------------------------
# Get the number of cores for threaded build
ifndef NPROCS
	NPROCS:=1
	ifeq ($(OS), Linux)
		NPROCS:=$(shell nproc)
	endif
	ifeq ($(OS), NetBSD)
		NPROCS:=$(shell getconf NPROCESSORS_ONLN)
	endif
	ifneq (,$(filter $(OS),Darwin FreeBSD))
		NPROCS:=$(shell sysctl -n hw.ncpu)
	endif
endif


# -----------------------------------------------------------------------------
# Macro for configuring cmake

CMAKE_CONFIG = cmake $(CMAKE_CONFIG_ARGS) \
                     -H"$(BLENDER_DIR)" \
                     -B"$(BUILD_DIR)" \
                     -DCMAKE_BUILD_TYPE_INIT:STRING=$(BUILD_TYPE)


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
all: .FORCE
	@echo
	@echo Configuring Blender in \"$(BUILD_DIR)\" ...

#	# if test ! -f $(BUILD_DIR)/CMakeCache.txt ; then \
#	# 	$(CMAKE_CONFIG); \
#	# fi

#	# do this always incase of failed initial build, could be smarter here...
	@$(CMAKE_CONFIG)

	@echo
	@echo Building Blender ...
	$(BUILD_COMMAND) -C "$(BUILD_DIR)" -j $(NPROCS) install
	@echo
	@echo edit build configuration with: "$(BUILD_DIR)/CMakeCache.txt" run make again to rebuild.
	@echo Blender successfully built, run from: $(BLENDER_BIN)
	@echo

debug: all
full: all
lite: all
release: all
cycles: all
headless: all
bpy: all
developer: all
ninja: all

# -----------------------------------------------------------------------------
# Build dependencies
DEPS_TARGET = install
ifneq "$(findstring clean, $(MAKECMDGOALS))" ""
	DEPS_TARGET = clean
endif

deps: .FORCE
	@echo
	@echo Configuring dependencies in \"$(DEPS_BUILD_DIR)\"

	@cmake -H"$(DEPS_SOURCE_DIR)" \
	       -B"$(DEPS_BUILD_DIR)" \
		   -DHARVEST_TARGET=$(DEPS_INSTALL_DIR)

	@echo
	@echo Building dependencies ...
	$(DEPS_BUILD_COMMAND) -C "$(DEPS_BUILD_DIR)" -j $(NPROCS) $(DEPS_TARGET)
	@echo
	@echo Dependencies successfully built and installed to $(DEPS_INSTALL_DIR).
	@echo

# -----------------------------------------------------------------------------
# Configuration (save some cd'ing around)
config: .FORCE
	$(CMAKE_CONFIG_TOOL) "$(BUILD_DIR)"


# -----------------------------------------------------------------------------
# Help for build targets
export HELP_TEXT
help: .FORCE
	@echo "$$HELP_TEXT"

# -----------------------------------------------------------------------------
# Packages
#
package_debian: .FORCE
	cd build_files/package_spec ; DEB_BUILD_OPTIONS="parallel=$(NPROCS)" sh ./build_debian.sh

package_pacman: .FORCE
	cd build_files/package_spec/pacman ; MAKEFLAGS="-j$(NPROCS)" makepkg

package_archive: .FORCE
	make -C "$(BUILD_DIR)" -s package_archive
	@echo archive in "$(BUILD_DIR)/release"


# -----------------------------------------------------------------------------
# Tests
#
test: .FORCE
	$(PYTHON) ./build_files/utils/make_test.py "$(BUILD_DIR)"

# run pep8 check check on scripts we distribute.
test_pep8: .FORCE
	$(PYTHON) tests/python/pep8.py > test_pep8.log 2>&1
	@echo "written: test_pep8.log"

# run some checks on our cmakefiles.
test_cmake: .FORCE
	$(PYTHON) build_files/cmake/cmake_consistency_check.py > test_cmake_consistency.log 2>&1
	@echo "written: test_cmake_consistency.log"

# run deprecation tests, see if we have anything to remove.
test_deprecated: .FORCE
	$(PYTHON) tests/check_deprecated.py


# -----------------------------------------------------------------------------
# Project Files
#

project_qtcreator: .FORCE
	$(PYTHON) build_files/cmake/cmake_qtcreator_project.py --build-dir "$(BUILD_DIR)"

project_netbeans: .FORCE
	$(PYTHON) build_files/cmake/cmake_netbeans_project.py "$(BUILD_DIR)"

project_eclipse: .FORCE
	cmake -G"Eclipse CDT4 - Unix Makefiles" -H"$(BLENDER_DIR)" -B"$(BUILD_DIR)"


# -----------------------------------------------------------------------------
# Static Checking
#

check_cppcheck: .FORCE
	$(CMAKE_CONFIG)
	cd "$(BUILD_DIR)" ; \
	$(PYTHON) "$(BLENDER_DIR)/build_files/cmake/cmake_static_check_cppcheck.py" 2> \
	    "$(BLENDER_DIR)/check_cppcheck.txt"
	@echo "written: check_cppcheck.txt"

check_clang_array: .FORCE
	$(CMAKE_CONFIG)
	cd "$(BUILD_DIR)" ; \
	$(PYTHON) "$(BLENDER_DIR)/build_files/cmake/cmake_static_check_clang_array.py"

check_splint: .FORCE
	$(CMAKE_CONFIG)
	cd "$(BUILD_DIR)" ; \
	$(PYTHON) "$(BLENDER_DIR)/build_files/cmake/cmake_static_check_splint.py"

check_sparse: .FORCE
	$(CMAKE_CONFIG)
	cd "$(BUILD_DIR)" ; \
	$(PYTHON) "$(BLENDER_DIR)/build_files/cmake/cmake_static_check_sparse.py"

check_smatch: .FORCE
	$(CMAKE_CONFIG)
	cd "$(BUILD_DIR)" ; \
	$(PYTHON) "$(BLENDER_DIR)/build_files/cmake/cmake_static_check_smatch.py"

check_spelling_py: .FORCE
	cd "$(BUILD_DIR)" ; \
	PYTHONIOENCODING=utf_8 $(PYTHON) \
	    "$(BLENDER_DIR)/source/tools/check_source/check_spelling.py" \
	    "$(BLENDER_DIR)/release/scripts"

check_spelling_c: .FORCE
	cd "$(BUILD_DIR)" ; \
	PYTHONIOENCODING=utf_8 $(PYTHON) \
	    "$(BLENDER_DIR)/source/tools/check_source/check_spelling.py" \
	    "$(BLENDER_DIR)/source" \
	    "$(BLENDER_DIR)/intern/cycles" \
	    "$(BLENDER_DIR)/intern/guardedalloc" \
	    "$(BLENDER_DIR)/intern/ghost" \

check_spelling_c_qtc: .FORCE
	cd "$(BUILD_DIR)" ; USE_QTC_TASK=1 \
	PYTHONIOENCODING=utf_8 $(PYTHON) \
	    "$(BLENDER_DIR)/source/tools/check_source/check_spelling.py" \
	    "$(BLENDER_DIR)/source" \
	    "$(BLENDER_DIR)/intern/cycles" \
	    "$(BLENDER_DIR)/intern/guardedalloc" \
	    "$(BLENDER_DIR)/intern/ghost" \
	    > \
	    "$(BLENDER_DIR)/check_spelling_c.tasks"

check_spelling_osl: .FORCE
	cd "$(BUILD_DIR)" ;\
	PYTHONIOENCODING=utf_8 $(PYTHON) \
	    "$(BLENDER_DIR)/source/tools/check_source/check_spelling.py" \
	    "$(BLENDER_DIR)/intern/cycles/kernel/shaders"

check_descriptions: .FORCE
	$(BLENDER_BIN) --background -noaudio --factory-startup --python \
	    "$(BLENDER_DIR)/source/tools/check_source/check_descriptions.py"

# -----------------------------------------------------------------------------
# Utilities
#

source_archive: .FORCE
	./build_files/utils/make_source_archive.sh

INKSCAPE_BIN?="inkscape"
icons: .FORCE
	BLENDER_BIN=$(BLENDER_BIN) INKSCAPE_BIN=$(INKSCAPE_BIN) \
		"$(BLENDER_DIR)/release/datafiles/blender_icons_update.py"
	BLENDER_BIN=$(BLENDER_BIN) INKSCAPE_BIN=$(INKSCAPE_BIN) \
		"$(BLENDER_DIR)/release/datafiles/prvicons_update.py"

icons_geom: .FORCE
	BLENDER_BIN=$(BLENDER_BIN) \
	    "$(BLENDER_DIR)/release/datafiles/blender_icons_geom_update.py"

update: .FORCE
	$(PYTHON) ./build_files/utils/make_update.py

format: .FORCE
	PATH="../lib/${OS_NCASE}_${CPU}/llvm/bin/:../lib/${OS_NCASE}_centos7_${CPU}/llvm/bin/:../lib/${OS_NCASE}/llvm/bin/:$(PATH)" \
		$(PYTHON) source/tools/utils_maintenance/clang_format_paths.py $(PATHS)


# -----------------------------------------------------------------------------
# Documentation
#

# Simple version of ./doc/python_api/sphinx_doc_gen.sh with no PDF generation.
doc_py: .FORCE
	ASAN_OPTIONS=halt_on_error=0 \
	$(BLENDER_BIN) --background -noaudio --factory-startup \
		--python doc/python_api/sphinx_doc_gen.py
	cd doc/python_api ; sphinx-build -b html sphinx-in sphinx-out
	@echo "docs written into: '$(BLENDER_DIR)/doc/python_api/sphinx-out/index.html'"

doc_doxy: .FORCE
	cd doc/doxygen; doxygen Doxyfile
	@echo "docs written into: '$(BLENDER_DIR)/doc/doxygen/html/index.html'"

doc_dna: .FORCE
	$(BLENDER_BIN) --background -noaudio --factory-startup \
		--python doc/blender_file_format/BlendFileDnaExporter_25.py
	@echo "docs written into: '$(BLENDER_DIR)/doc/blender_file_format/dna.html'"

doc_man: .FORCE
	$(PYTHON) doc/manpage/blender.1.py $(BLENDER_BIN) blender.1

help_features: .FORCE
	@$(PYTHON) "$(BLENDER_DIR)/build_files/cmake/cmake_print_build_options.py" $(BLENDER_DIR)"/CMakeLists.txt"

clean: .FORCE
	$(BUILD_COMMAND) -C "$(BUILD_DIR)" clean

.PHONY: all

.FORCE:
