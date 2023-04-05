# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2014 Blender Foundation

set(INC
  .
  ..
  ../..
  ../../../makesdna
  ../../../../../intern/atomic
  ../../../../../intern/guardedalloc
)

include_directories(${INC})

blender_test_performance(BLI_ghash_performance "bf_blenlib")
blender_test_performance(BLI_task_performance "bf_blenlib")
