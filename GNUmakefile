# SPDX-License-Identifier: GPL-2.0-or-later

# This Makefile does an out-of-source CMake build in ../build_`OS`
# eg:
#   ../build_linux_i386
# This is for users who like to configure & build blender with a single command.

define HELP_TEXT

Blender Convenience Targets
   Provided for building Blender (multiple targets can be used at once).

   * debug:         Build a debug binary.
   * full:          Enable all supported dependencies & options.
   * lite:          Disable non essential features for a smaller binary and faster build.
   * release:       Complete build with all options enabled including CUDA and Optix, matching the releases on blender.org
   * headless:      Build without an interface (renderfarm or server automation).
   * cycles:        Build Cycles standalone only, without Blender.
   * bpy:           Build as a python module which can be loaded from python directly.
   * developer:     Enable faster builds, error checking and tests, recommended for developers.
   * ninja:         Use ninja build tool for faster builds.
   * ccache:        Use ccache for faster rebuilds.

   Note: when passing in multiple targets their order is not important.
   So for a fast build you can for e.g. run 'make lite ccache ninja'.
   Note: passing the argument 'BUILD_DIR=path' when calling make will override the default build dir.
   Note: passing the argument 'BUILD_CMAKE_ARGS=args' lets you add cmake arguments.

Other Convenience Targets
   Provided for other building operations.

   * config:        Run cmake configuration tool to set build options.
   * deps:          Build library dependencies (intended only for platform maintainers).

                    The existence of locally build dependencies overrides the pre-built dependencies from subversion.
                    These must be manually removed from 'lib/' to go back to using the pre-compiled libraries.

Project Files
   Generate project files for development environments.

   * project_qtcreator:     QtCreator Project Files.
   * project_netbeans:      NetBeans Project Files.
   * project_eclipse:       Eclipse CDT4 Project Files.

Package Targets

   * package_archive:   Build an archive package.

Testing Targets
   Not associated with building Blender.

   * test:
     Run automated tests with ctest.

Static Source Code Checking
   Not associated with building Blender.

   * check_cppcheck:        Run blender source through cppcheck (C & C++).
   * check_clang_array:     Run blender source through clang array checking script (C & C++).
   * check_deprecated:      Check if there is any deprecated code to remove.
   * check_splint:          Run blenders source through splint (C only).
   * check_sparse:          Run blenders source through sparse (C only).
   * check_smatch:          Run blenders source through smatch (C only).
   * check_descriptions:    Check for duplicate/invalid descriptions.
   * check_licenses:        Check license headers follow the SPDX license specification,
                            using one of the accepted licenses in 'doc/license/SPDX-license-identifiers.txt'
                            Append with 'SHOW_HEADERS=1' to show all unique headers
                            which can be useful for spotting license irregularities.
   * check_cmake:           Runs our own cmake file checker which detects errors in the cmake file list definitions.
   * check_pep8:            Checks all Python script are pep8 which are tagged to use the stricter formatting.
   * check_mypy:            Checks all Python scripts using mypy,
                            see: source/tools/check_source/check_mypy_config.py scripts which are included.

Spell Checkers
   This runs the spell checker from the developer tools repositor.

   * check_spelling_c:      Check for spelling errors (C/C++ only),
   * check_spelling_osl:    Check for spelling errors (OSL only).
   * check_spelling_py:     Check for spelling errors (Python only).

   Note: an additional word-list is maintained at: 'source/tools/check_source/check_spelling_c_config.py'

   Note: that spell checkers can take a 'CHECK_SPELLING_CACHE' filepath argument,
   so re-running does not need to re-check unchanged files.

   Example:
      make check_spelling_c CHECK_SPELLING_CACHE=../spelling_cache.data

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

   * source_archive_complete:
     Create a compressed archive of the source code and all the libraries of dependencies.

   * update:
     Updates git and all submodules and svn.

   * update_code:
     Updates git and all submodules but not svn.

   * format:
     Format source code using clang-format & autopep8 (uses PATHS if passed in). For example::

        make format PATHS="source/blender/blenlib source/blender/blenkernel"

Environment Variables

   * BUILD_CMAKE_ARGS:      Arguments passed to CMake.
   * BUILD_DIR:             Override default build path.
   * PYTHON:                Use this for the Python command (used for checking tools).
   * NPROCS:                Number of processes to use building (auto-detect when omitted).
   * AUTOPEP8:              Command used for Python code-formatting (used for the format target).

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

# Use our OS and CPU architecture naming conventions.
ifeq ($(CPU),x86_64)
	CPU:=x64
endif
ifeq ($(OS_NCASE),darwin)
	OS_LIBDIR:=macos
else
	OS_LIBDIR:=$(OS_NCASE)
endif


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
	DEPS_INSTALL_DIR:=$(shell dirname "$(BLENDER_DIR)")/lib/$(OS_LIBDIR)_$(CPU)
endif

# Set the LIBDIR, an empty string when not found.
LIBDIR:=$(wildcard $(BLENDER_DIR)/lib/${OS_LIBDIR}_${CPU})
ifeq (, $(LIBDIR))
	LIBDIR:=$(wildcard $(BLENDER_DIR)/lib/${OS_LIBDIR})
endif

# Find the newest Python version bundled in `LIBDIR`.
PY_LIB_VERSION:=3.15
ifeq (, $(wildcard $(LIBDIR)/python/bin/python$(PY_LIB_VERSION)))
	PY_LIB_VERSION:=3.14
	ifeq (, $(wildcard $(LIBDIR)/python/bin/python$(PY_LIB_VERSION)))
		PY_LIB_VERSION:=3.13
		ifeq (, $(wildcard $(LIBDIR)/python/bin/python$(PY_LIB_VERSION)))
			PY_LIB_VERSION:=3.12
			ifeq (, $(wildcard $(LIBDIR)/python/bin/python$(PY_LIB_VERSION)))
				PY_LIB_VERSION:=3.11
				ifeq (, $(wildcard $(LIBDIR)/python/bin/python$(PY_LIB_VERSION)))
					PY_LIB_VERSION:=3.10
				endif
			endif
		endif
	endif
endif

# Allow to use alternative binary (pypy3, etc)
ifndef PYTHON
	# If not overriden, first try using Python from LIBDIR.
	PYTHON:=$(LIBDIR)/python/bin/python$(PY_LIB_VERSION)
	ifeq (, $(wildcard $(PYTHON)))
		# If not available, use system python3 or python command.
		PYTHON:=python3
		ifeq (, $(shell command -v $(PYTHON)))
			PYTHON:=python
		endif
	else
		# Don't generate __pycache__ files in lib folder, they
		# can interfere with updates.
		PYTHON:=$(PYTHON) -B
	endif
endif

# Use the autopep8 module in ../lib/ (which can be executed via Python directly).
# Otherwise the "autopep8" command can be used.
ifndef AUTOPEP8
	ifneq (, $(LIBDIR))
		AUTOPEP8:=$(wildcard $(LIBDIR)/python/lib/python$(PY_LIB_VERSION)/site-packages/autopep8.py)
	endif
	ifeq (, $(AUTOPEP8))
		AUTOPEP8:=autopep8
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

ifneq "$(findstring ccache, $(MAKECMDGOALS))" ""
	CMAKE_CONFIG_ARGS:=-DWITH_COMPILER_CCACHE=YES $(CMAKE_CONFIG_ARGS)
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
		ifeq ($(OS), Darwin)
			DEPS_BUILD_COMMAND:=make -s
		else
			DEPS_BUILD_COMMAND:="$(BLENDER_DIR)/build_files/build_environment/linux/make_deps_wrapper.sh" -s
		endif
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

# X11 specific.
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
ccache: all

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

package_archive: .FORCE
	make -C "$(BUILD_DIR)" -s package_archive
	@echo archive in "$(BUILD_DIR)/release"


# -----------------------------------------------------------------------------
# Tests
#
test: .FORCE
	@$(PYTHON) ./build_files/utils/make_test.py "$(BUILD_DIR)"


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
	@$(CMAKE_CONFIG)
	@cd "$(BUILD_DIR)" ; \
	$(PYTHON) \
	    "$(BLENDER_DIR)/build_files/cmake/cmake_static_check_cppcheck.py" 2> \
	    "$(BLENDER_DIR)/check_cppcheck.txt"
	@echo "written: check_cppcheck.txt"

check_clang_array: .FORCE
	@$(CMAKE_CONFIG)
	@cd "$(BUILD_DIR)" ; \
	$(PYTHON) "$(BLENDER_DIR)/build_files/cmake/cmake_static_check_clang_array.py"

check_splint: .FORCE
	@$(CMAKE_CONFIG)
	@cd "$(BUILD_DIR)" ; \
	$(PYTHON) "$(BLENDER_DIR)/build_files/cmake/cmake_static_check_splint.py"

check_sparse: .FORCE
	@$(CMAKE_CONFIG)
	@cd "$(BUILD_DIR)" ; \
	$(PYTHON) "$(BLENDER_DIR)/build_files/cmake/cmake_static_check_sparse.py"

check_smatch: .FORCE
	@$(CMAKE_CONFIG)
	@cd "$(BUILD_DIR)" ; \
	$(PYTHON) "$(BLENDER_DIR)/build_files/cmake/cmake_static_check_smatch.py"

check_mypy: .FORCE
	@$(PYTHON) "$(BLENDER_DIR)/source/tools/check_source/check_mypy.py"

check_spelling_py: .FORCE
	@cd "$(BUILD_DIR)" ; \
	PYTHONIOENCODING=utf_8 $(PYTHON) \
	    "$(BLENDER_DIR)/source/tools/check_source/check_spelling.py" \
	    "$(BLENDER_DIR)/release/scripts"

check_spelling_c: .FORCE
	@cd "$(BUILD_DIR)" ; \
	PYTHONIOENCODING=utf_8 $(PYTHON) \
	    "$(BLENDER_DIR)/source/tools/check_source/check_spelling.py" \
	    --cache-file=$(CHECK_SPELLING_CACHE) \
	    "$(BLENDER_DIR)/source" \
	    "$(BLENDER_DIR)/intern/cycles" \
	    "$(BLENDER_DIR)/intern/guardedalloc" \
	    "$(BLENDER_DIR)/intern/ghost" \

check_spelling_osl: .FORCE
	@cd "$(BUILD_DIR)" ; \
	PYTHONIOENCODING=utf_8 $(PYTHON) \
	    "$(BLENDER_DIR)/source/tools/check_source/check_spelling.py" \
	    --cache-file=$(CHECK_SPELLING_CACHE) \
	    "$(BLENDER_DIR)/intern/cycles/kernel/shaders"

check_descriptions: .FORCE
	@$(BLENDER_BIN) --background -noaudio --factory-startup --python \
	    "$(BLENDER_DIR)/source/tools/check_source/check_descriptions.py"

check_deprecated: .FORCE
	@PYTHONIOENCODING=utf_8 $(PYTHON) \
	    source/tools/check_source/check_deprecated.py

check_licenses: .FORCE
	@PYTHONIOENCODING=utf_8 $(PYTHON) \
	    "$(BLENDER_DIR)/source/tools/check_source/check_licenses.py" \
	    "--show-headers=$(SHOW_HEADERS)"

check_pep8: .FORCE
	@PYTHONIOENCODING=utf_8 $(PYTHON) \
	    tests/python/pep8.py

check_cmake: .FORCE
	@PYTHONIOENCODING=utf_8 $(PYTHON) \
	    source/tools/check_source/check_cmake_consistency.py


# -----------------------------------------------------------------------------
# Utilities
#

source_archive: .FORCE
	@$(PYTHON) ./build_files/utils/make_source_archive.py

source_archive_complete: .FORCE
	@cmake \
	    -S "$(BLENDER_DIR)/build_files/build_environment" -B"$(BUILD_DIR)/source_archive" \
	    -DCMAKE_BUILD_TYPE_INIT:STRING=$(BUILD_TYPE) -DPACKAGE_USE_UPSTREAM_SOURCES=OFF
# This assumes CMake is still using a default `PACKAGE_DIR` variable:
	@$(PYTHON) ./build_files/utils/make_source_archive.py --include-packages "$(BUILD_DIR)/source_archive/packages"


INKSCAPE_BIN?="inkscape"
icons: .FORCE
	@BLENDER_BIN=$(BLENDER_BIN) INKSCAPE_BIN=$(INKSCAPE_BIN) \
	    "$(BLENDER_DIR)/release/datafiles/blender_icons_update.py"
	@INKSCAPE_BIN=$(INKSCAPE_BIN) \
	    "$(BLENDER_DIR)/release/datafiles/prvicons_update.py"
	@INKSCAPE_BIN=$(INKSCAPE_BIN) \
	    "$(BLENDER_DIR)/release/datafiles/alert_icons_update.py"

icons_geom: .FORCE
	@BLENDER_BIN=$(BLENDER_BIN) \
	    "$(BLENDER_DIR)/release/datafiles/blender_icons_geom_update.py"

update: .FORCE
	@$(PYTHON) ./build_files/utils/make_update.py

update_code: .FORCE
	@$(PYTHON) ./build_files/utils/make_update.py --no-libraries

format: .FORCE
	@PATH="${LIBDIR}/llvm/bin/:$(PATH)" $(PYTHON) source/tools/utils_maintenance/clang_format_paths.py $(PATHS)
	@$(PYTHON) source/tools/utils_maintenance/autopep8_format_paths.py --autopep8-command="$(AUTOPEP8)" $(PATHS)


# -----------------------------------------------------------------------------
# Documentation
#

# Simple version of ./doc/python_api/sphinx_doc_gen.sh with no PDF generation.
doc_py: .FORCE
	@ASAN_OPTIONS=halt_on_error=0:${ASAN_OPTIONS} \
	$(BLENDER_BIN) \
	    --background -noaudio --factory-startup \
	    --python doc/python_api/sphinx_doc_gen.py
	@sphinx-build -b html -j $(NPROCS) doc/python_api/sphinx-in doc/python_api/sphinx-out
	@echo "docs written into: '$(BLENDER_DIR)/doc/python_api/sphinx-out/index.html'"

doc_doxy: .FORCE
	@cd doc/doxygen; doxygen Doxyfile
	@echo "docs written into: '$(BLENDER_DIR)/doc/doxygen/html/index.html'"

doc_dna: .FORCE
	@$(BLENDER_BIN) \
	    --background -noaudio --factory-startup \
	    --python doc/blender_file_format/BlendFileDnaExporter_25.py
	@echo "docs written into: '$(BLENDER_DIR)/doc/blender_file_format/dna.html'"

doc_man: .FORCE
	@$(PYTHON) doc/manpage/blender.1.py --blender="$(BLENDER_BIN)" --output=blender.1 --verbose

help_features: .FORCE
	@$(PYTHON) "$(BLENDER_DIR)/build_files/cmake/cmake_print_build_options.py" $(BLENDER_DIR)"/CMakeLists.txt"

clean: .FORCE
	$(BUILD_COMMAND) -C "$(BUILD_DIR)" clean

.PHONY: all

.FORCE:
