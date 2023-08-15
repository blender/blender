# SPDX-FileCopyrightText: 2014 Blender Authors
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

blender_add_performancetest_executable(BLI_ghash_performance "BLI_ghash_performance_test.cc" "${INC}" "${INC_SYS}" "${LIB}")
blender_add_performancetest_executable(BLI_task_performance "BLI_task_performance_test.cc" "${INC}" "${INC_SYS}" "${LIB}")
