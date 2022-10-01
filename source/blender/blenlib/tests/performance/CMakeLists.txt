# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2014 Blender Foundation. All rights reserved.

set(INC
  .
  ..
  ../..
  ../../../makesdna
  ../../../../../intern/guardedalloc
  ../../../../../intern/atomic
)

include_directories(${INC})

blender_test_performance(BLI_ghash_performance "bf_blenlib")
blender_test_performance(BLI_task_performance "bf_blenlib")
