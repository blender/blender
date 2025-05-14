# SPDX-FileCopyrightText: 2011-2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
  intern
  ../animrig
  ../makesrna

  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS

)

set(SRC
  SEQ_add.hh
  SEQ_animation.hh
  SEQ_channels.hh
  SEQ_connect.hh
  SEQ_edit.hh
  SEQ_effects.hh
  SEQ_iterator.hh
  SEQ_modifier.hh
  SEQ_offscreen.hh
  SEQ_prefetch.hh
  SEQ_proxy.hh
  SEQ_relations.hh
  SEQ_render.hh
  SEQ_retiming.hh
  SEQ_select.hh
  SEQ_sequencer.hh
  SEQ_sound.hh
  SEQ_thumbnail_cache.hh
  SEQ_time.hh
  SEQ_transform.hh
  SEQ_utils.hh

  intern/animation.cc
  intern/cache/final_image_cache.cc
  intern/cache/final_image_cache.hh
  intern/cache/intra_frame_cache.cc
  intern/cache/intra_frame_cache.hh
  intern/cache/source_image_cache.cc
  intern/cache/source_image_cache.hh
  intern/cache/thumbnail_cache.cc
  intern/channels.cc
  intern/effects/effects.cc
  intern/effects/effects.hh
  intern/effects/vse_effect_add_sub_mul.cc
  intern/effects/vse_effect_adjustment.cc
  intern/effects/vse_effect_blend.cc
  intern/effects/vse_effect_cross.cc
  intern/effects/vse_effect_gaussian_blur.cc
  intern/effects/vse_effect_glow.cc
  intern/effects/vse_effect_multi_camera.cc
  intern/effects/vse_effect_solid_color.cc
  intern/effects/vse_effect_speed.cc
  intern/effects/vse_effect_text.cc
  intern/effects/vse_effect_transform.cc
  intern/effects/vse_effect_wipe.cc
  intern/iterator.cc
  intern/media_presence.cc
  intern/modifier.cc
  intern/multiview.cc
  intern/multiview.hh
  intern/prefetch.cc
  intern/prefetch.hh
  intern/proxy.cc
  intern/proxy.hh
  intern/proxy_job.cc
  intern/render.cc
  intern/render.hh
  intern/sequencer.cc
  intern/sequencer.hh
  intern/sound.cc
  intern/strip_add.cc
  intern/strip_connect.cc
  intern/strip_edit.cc
  intern/strip_lookup.cc
  intern/strip_relations.cc
  intern/strip_retiming.cc
  intern/strip_select.cc
  intern/strip_time.cc
  intern/strip_time.hh
  intern/strip_transform.cc
  intern/utils.cc
  intern/utils.hh
)

set(LIB
  PRIVATE bf::blenfont
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blenloader
  PRIVATE bf::blentranslation
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::imbuf
  PRIVATE bf::imbuf::movie
  PRIVATE bf::intern::atomic
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::render
  PRIVATE bf::windowmanager
)

if(WITH_AUDASPACE)
  list(APPEND INC_SYS
    ${AUDASPACE_C_INCLUDE_DIRS}
  )
  if(WITH_SYSTEM_AUDASPACE)
    list(APPEND LIB
      ${AUDASPACE_C_LIBRARIES}
      ${AUDASPACE_PY_LIBRARIES}
    )
  endif()
  add_definitions(-DWITH_AUDASPACE)

  if(WITH_FFTW3)
    add_definitions(-DWITH_CONVOLUTION)
  endif()
endif()

blender_add_lib(bf_sequencer "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::sequencer ALIAS bf_sequencer)

# RNA_prototypes.hh
add_dependencies(bf_sequencer bf_rna)
