# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later


# -----------------------------------------------------------------------------
# Build `datatoc` executable.
set(SRC
  datatoc.cc
)

# When cross-compiling the executable is separately compiled for the host architecture
# in a sub-build defined in host_tools.cmake. As this is a build-time code-gen tool
# that must run on the host build machine, the executable is then provided by the
# DATATOC_EXECUTABLE variable
if(NOT CMAKE_CROSSCOMPILING)
  # `SRC_DNA_INC` is defined in the parent directory.
  add_executable(datatoc ${SRC})
  optimize_debug_target(datatoc)
endif()
