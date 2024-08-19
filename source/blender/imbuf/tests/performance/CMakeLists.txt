# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../..
)

set(INC_SYS
)

set(LIB
  PRIVATE bf_blenlib
  PRIVATE bf_imbuf
)

set(SRC
  IMB_scaling_performance_test.cc
)

blender_add_test_performance_executable(IMB_performance "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
if(WITH_BUILDINFO)
  target_link_libraries(IMB_performance_test PRIVATE buildinfoobj)
endif()
