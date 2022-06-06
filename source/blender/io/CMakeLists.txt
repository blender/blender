# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2020 Blender Foundation. All rights reserved.

add_subdirectory(common)
add_subdirectory(wavefront_obj)
add_subdirectory(stl)

if(WITH_ALEMBIC)
  add_subdirectory(alembic)
endif()

if(WITH_CODEC_AVI)
  add_subdirectory(avi)
endif()

if(WITH_OPENCOLLADA)
  add_subdirectory(collada)
endif()

if(WITH_USD)
  add_subdirectory(usd)
endif()

add_subdirectory(gpencil)
