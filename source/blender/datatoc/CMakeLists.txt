# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later


# -----------------------------------------------------------------------------
# Build datatoc executable
set(SRC
  datatoc.cc
)

# SRC_DNA_INC is defined in the parent dir
add_executable(datatoc ${SRC})
