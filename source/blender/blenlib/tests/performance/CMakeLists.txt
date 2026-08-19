# SPDX-FileCopyrightText: 2014-2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ..
  ../..
)

set(INC_SYS
)

set(LIB
  PRIVATE bf_blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::intern::atomic
)

set(SRC
  BLI_map_performance_test.cc
)

blender_add_test_performance_executable(BLI_map_performance "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

set(SRC
  BLI_group_indices.cc
)

blender_add_test_performance_executable(BLI_group_indices "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
