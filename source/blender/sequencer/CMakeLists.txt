# SPDX-FileCopyrightText: 2011 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  intern
  ../blenfont
  ../blenkernel
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

  intern/animation.c
  intern/channels.c
  intern/clipboard.c
  intern/disk_cache.c
  intern/disk_cache.h
  intern/effects.c
  intern/effects.h
  intern/image_cache.c
  intern/image_cache.h
  intern/iterator.c
  intern/modifier.c
  intern/multiview.c
  intern/multiview.h
  intern/prefetch.c
  intern/prefetch.h
  intern/proxy.c
  intern/proxy.h
  intern/proxy_job.c
  intern/render.c
  intern/render.h
  intern/sequence_lookup.c
  intern/sequencer.c
  intern/sequencer.h
  intern/sound.c
  intern/strip_add.c
  intern/strip_edit.c
  intern/strip_relations.c
  intern/strip_retiming.cc
  intern/strip_select.c
  intern/strip_time.c
  intern/strip_time.h
  intern/strip_transform.c
  intern/utils.c
  intern/utils.h
)

set(LIB
  bf_blenkernel
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
