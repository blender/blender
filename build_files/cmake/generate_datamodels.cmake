# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(GENERATE_DATAMODELS_SCRIPT "${CMAKE_SOURCE_DIR}/tools/utils/make_generate_datamodels.py")

add_custom_target(generate_datamodels
    COMMAND ${PYTHON_EXECUTABLE} ${GENERATE_DATAMODELS_SCRIPT} ${CMAKE_SOURCE_DIR}
    DEPENDS ${GENERATE_DATAMODELS_SCRIPT}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating datamodels"
    VERBATIM
)
