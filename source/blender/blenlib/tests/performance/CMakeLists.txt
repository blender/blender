# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2014 Blender Foundation. All rights reserved.

set(INC
  .
  ..
)

include_directories(${INC})

BLENDER_TEST_PERFORMANCE(BLI_ghash_performance "bf_blenlib")
BLENDER_TEST_PERFORMANCE(BLI_task_performance "bf_blenlib")
