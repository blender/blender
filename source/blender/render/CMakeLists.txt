# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later


set(INC
  .
  intern
  ../blenkernel
  ../blentranslation
  ../compositor
  ../compositor/realtime_compositor
  ../compositor/realtime_compositor/cached_resources
  ../draw
  ../draw/intern
  ../gpu
  ../gpu/intern
  ../imbuf
  ../makesrna
  ../nodes
  ../sequencer
  ../simulation
  ../windowmanager
  ../../../intern/mikktspace
  ../../../intern/mantaflow/extern
)

set(INC_SYS
)

set(SRC
  intern/bake.cc
  intern/compositor.cc
  intern/engine.cc
  intern/initrender.cc
  intern/multires_bake.cc
  intern/pipeline.cc
  intern/render_result.cc
  intern/render_types.cc
  intern/texture_image.cc
  intern/texture_margin.cc
  intern/texture_pointdensity.cc
  intern/texture_procedural.cc
  intern/tile_highlight.cc
  intern/zbuf.cc

  RE_bake.h
  RE_compositor.hh
  RE_engine.h
  RE_multires_bake.h
  RE_pipeline.h
  RE_texture.h
  RE_texture_margin.h

  intern/pipeline.hh
  intern/render_result.h
  intern/render_types.h
  intern/texture_common.h
  intern/tile_highlight.h
  intern/zbuf.h
)

set(LIB
  PRIVATE bf::blenlib
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  bf_realtime_compositor
  PRIVATE bf::intern::atomic
)

if(WITH_PYTHON)
  add_definitions(-DWITH_PYTHON)
  list(APPEND INC
    ../python
  )
endif()

if(WITH_IMAGE_OPENEXR)
  list(APPEND LIB
    bf_imbuf_openexr
  )
  add_definitions(-DWITH_OPENEXR)
endif()

if(WITH_FREESTYLE)
  list(APPEND INC
    ../freestyle
  )
  list(APPEND LIB
    bf_freestyle
  )
  add_definitions(-DWITH_FREESTYLE)
endif()


blender_add_lib_nolist(bf_render "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

if(WITH_HYDRA)
  add_subdirectory(hydra)
endif()
