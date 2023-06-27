# SPDX-FileCopyrightText: 2014 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ..
  ../..
  ../../../makesdna
  ../../../../../intern/atomic
  ../../../../../intern/guardedalloc
)

set(INC_SYS
)

set(LIB
  PRIVATE bf_blenlib
  PRIVATE bf_intern_guardedalloc
  PRIVATE bf_intern_atomic
)

blender_add_performancetest_executable(BLI_ghash_performance "BLI_ghash_performance_test.cc" "${INC}" "${INC_SYS}" "${LIB}")
blender_add_performancetest_executable(BLI_task_performance "BLI_task_performance_test.cc" "${INC}" "${INC_SYS}" "${LIB}")
