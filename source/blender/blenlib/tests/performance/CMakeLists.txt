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
  PRIVATE PRIVATE bf::intern::guardedalloc
  PRIVATE bf::intern::atomic
)

blender_add_test_performance_executable(BLI_map_performance "BLI_map_performance_test.cc" "${INC}" "${INC_SYS}" "${LIB}")
