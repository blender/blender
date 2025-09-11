# SPDX-FileCopyrightText: 2011-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# note: this isn't needed for building,
# its just for some convenience targets.

define HELP_TEXT
Checkers:

- check_mypy, watch_check_mypy
  Runs the mypy checker, optionally watching files & re-running.

- check_ruff, watch_check_ruff
  Runs the ruff checker, optionally watching files & re-running.

- check_pylint, watch_check_pylint
  Runs the pylint checker, optionally watching files & re-running.

- check_flake8, watch_check_flake8


Tests:

- test, watch_test
  Runs all tests listed below.

- test_cli
  Test the command line program, "blender_ext.py" Blender is not used.

- test_blender
  Test blender (currently only some limited tests).

- test_cli_blender
  Runs tests that access & manipulate extensions via the command line.


Environment Variables:

- BLENDER_BIN
  The command used to run Blender, defaults to "blender"

- PYTHON_BIN
  The command used to run PYthon, defaults to "python3"

endef
# HELP_TEXT (end)

# Needed for when tests are run from another directory: `make -C ./scripts/addons_core`
BASE_DIR := ${CURDIR}

PY_FILES=$(shell find ./ -type f -name '*.py')
# Filter out files which use `bpy`.
PY_FILES_MYPY=$(filter-out \
	./__init__.py \
	./bl_extension_cli.py \
	./bl_extension_monkeypatch.py \
	./bl_extension_notify.py \
	./bl_extension_ops.py \
	./bl_extension_ui.py \
	./bl_extension_utils.py \
	./extensions_map_from_legacy_addons.py \
	./example_extension/__init__.py \
	,$(PY_FILES))

PY_FILES_MYPY_STANDALONE= \
	./bl_extension_utils.py \
	./bl_extension_cli.py

EXTRA_WATCH_FILES=Makefile

# For tests that launch Blender directly.
BLENDER_BIN?=$(shell which blender)
PYTHON_BIN?=$(shell which python3)


# -----------------------------------------------------------------------------
# Help for build targets

export HELP_TEXT
help: FORCE
	@echo "$$HELP_TEXT"


# -----------------------------------------------------------------------------
# Checking Utilities

# `--no-namespace-packages` is needed otherwise `./cli/blender_ext.py` loads in parent modules
# (the Blender add-on which imports `bpy`).
check_mypy: FORCE
	@mypy --no-namespace-packages --strict $(PY_FILES_MYPY)
	@mypy --strict --follow-imports=skip $(PY_FILES_MYPY_STANDALONE)
watch_check_mypy:
	@cd "$(BASE_DIR)" && \
	while true; do \
		$(MAKE) check_mypy; \
		inotifywait -q -e close_write $(EXTRA_WATCH_FILES) \
		            $(PY_FILES_MYPY) \
		            ./bl_extension_utils.py ; \
		tput clear; \
	done

check_ruff: FORCE
	@cd "$(BASE_DIR)" && ruff check $(PY_FILES_MYPY)
	@cd "$(BASE_DIR)" && ruff check $(PY_FILES_MYPY_STANDALONE)
watch_check_ruff:
	@cd "$(BASE_DIR)" && \
	while true; do \
		$(MAKE) check_ruff; \
		inotifywait -q -e close_write $(EXTRA_WATCH_FILES) \
		            $(PY_FILES_MYPY) \
		            ./bl_extension_utils.py ; \
		tput clear; \
	done

check_pylint:
	@cd "$(BASE_DIR)" && \
	    pylint $(PY_FILES) \
	    --enable=useless-suppression \
	    --disable=C0103,C0111,C0201,C0301,C0302,C0415,R0401,R1702,R1705,R0902,R0903,R0913,E0611,E0401,I1101,R0801,C0209,W0511,W0718,W0719,C0413,R0911,R0912,R0914,R0915 \
	    --msg-template='{abspath}:{line}:{column}: {msg_id}: {msg} ({symbol})'
watch_check_pylint:
	@cd "$(BASE_DIR)" && \
	while true; do \
		$(MAKE) check_pylint; \
		inotifywait -q -e close_write $(EXTRA_WATCH_FILES) $(PY_FILES) ; \
		tput clear; \
	done


# -----------------------------------------------------------------------------
# Tests (All)

test: FORCE
	@$(MAKE) -C "$(BASE_DIR)" test_cli;
	@$(MAKE) -C "$(BASE_DIR)" test_blender;
	@$(MAKE) -C "$(BASE_DIR)" test_cli_blender;
watch_test: FORCE
	@cd "$(BASE_DIR)" && \
	while true; do \
		$(MAKE) test; \
		inotifywait -q -e close_write $(EXTRA_WATCH_FILES) $(PY_FILES) ; \
		tput clear; \
	done


# -----------------------------------------------------------------------------
# Tests (Individual)

# python3 ./tests/test_cli.py
test_cli: FORCE
	@cd "$(BASE_DIR)" && \
	    USE_HTTP=0 \
	    $(PYTHON_BIN) ./tests/test_cli.py
	@cd "$(BASE_DIR)" && \
	    USE_HTTP=1 \
	    $(PYTHON_BIN) ./tests/test_cli.py
watch_test_cli: FORCE
	@cd "$(BASE_DIR)" && \
	while true; do \
		$(MAKE) test_cli; \
		inotifywait -q -e close_write $(EXTRA_WATCH_FILES) $(PY_FILES) ; \
		tput clear; \
	done


# NOTE: these rely on the blender binary.
test_blender: FORCE
	@cd "$(BASE_DIR)" && \
	    ASAN_OPTIONS=check_initialization_order=0:leak_check_at_exit=0 \
	    $(BLENDER_BIN) \
	        --background --factory-startup --online-mode -noaudio \
	        --python ./tests/test_blender.py -- --verbose
watch_test_blender: FORCE
	@cd "$(BASE_DIR)" && \
	while true; do \
		$(MAKE) test_blender; \
		inotifywait -q -e close_write $(EXTRA_WATCH_FILES) $(PY_FILES) ; \
		tput clear; \
	done

test_cli_blender: FORCE
	@cd "$(BASE_DIR)" && \
	    env BLENDER_BIN=$(BLENDER_BIN) \
	    $(PYTHON_BIN) ./tests/test_cli_blender.py
watch_test_cli_blender: FORCE
	@cd "$(BASE_DIR)" && \
	while true; do \
		env BLENDER_BIN=$(BLENDER_BIN) \
		    $(MAKE) test_cli_blender; \
		inotifywait -q -e close_write $(EXTRA_WATCH_FILES) $(PY_FILES) ; \
		tput clear; \
	done

test_path_pattern_match: FORCE
	@cd "$(BASE_DIR)" && \
	    $(PYTHON_BIN) ./tests/test_path_pattern_match.py
watch_test_path_pattern_match: FORCE
	@cd "$(BASE_DIR)" && \
	while true; do \
		$(MAKE) test_path_pattern_match; \
		inotifywait -q -e close_write $(EXTRA_WATCH_FILES) $(PY_FILES) ; \
		tput clear; \
	done

FORCE:
