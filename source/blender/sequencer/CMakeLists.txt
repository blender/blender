# SPDX-FileCopyrightText: 2011 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  intern
  ../blenfont
  ../blenloader
  ../blentranslation
  ../depsgraph
  ../imbuf
  ../makesrna
  ../render
  ../windowmanager
  ../../../intern/clog

  # RNA_prototypes.h
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS

)

set(SRC
  SEQ_add.h
  SEQ_animation.h
  SEQ_channels.h
  SEQ_clipboard.h
  SEQ_edit.h
  SEQ_effects.h
  SEQ_iterator.h
  SEQ_modifier.h
  SEQ_prefetch.h
  SEQ_proxy.h
  SEQ_relations.h
  SEQ_render.h
  SEQ_retiming.h
  SEQ_retiming.hh
  SEQ_select.h
  SEQ_sequencer.h
  SEQ_sound.h
  SEQ_time.h
  SEQ_transform.h
  SEQ_utils.h

  intern/animation.cc
  intern/channels.cc
  intern/clipboard.cc
  intern/disk_cache.cc
  intern/disk_cache.h
  intern/effects.cc
  intern/effects.h
  intern/image_cache.cc
  intern/image_cache.h
  intern/iterator.cc
  intern/modifier.cc
  intern/multiview.cc
  intern/multiview.h
  intern/prefetch.cc
  intern/prefetch.h
  intern/proxy.cc
  intern/proxy.h
  intern/proxy_job.cc
  intern/render.cc
  intern/render.h
  intern/sequence_lookup.cc
  intern/sequencer.cc
  intern/sequencer.h
  intern/sound.cc
  intern/strip_add.cc
  intern/strip_edit.cc
  intern/strip_relations.cc
  intern/strip_retiming.cc
  intern/strip_select.cc
  intern/strip_time.cc
  intern/strip_time.h
  intern/strip_transform.cc
  intern/utils.cc
  intern/utils.h
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::atomic
  PRIVATE bf::intern::guardedalloc
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
endif()

blender_add_lib(bf_sequencer "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.h
add_dependencies(bf_sequencer bf_rna)
