# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2011, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  intern
  ../blenfont
  ../blenkernel
  ../blenlib
  ../blenloader
  ../blentranslation
  ../depsgraph
  ../imbuf
  ../makesdna
  ../makesrna
  ../render
  ../windowmanager
  ../../../intern/atomic
  ../../../intern/clog
  ../../../intern/guardedalloc

  # dna_type_offsets.h
  ${CMAKE_CURRENT_BINARY_DIR}/../makesdna/intern
)

set(INC_SYS

)

set(SRC
  SEQ_add.h
  SEQ_clipboard.h
  SEQ_edit.h
  SEQ_effects.h
  SEQ_iterator.h
  SEQ_modifier.h
  SEQ_prefetch.h
  SEQ_proxy.h
  SEQ_relations.h
  SEQ_render.h
  SEQ_select.h
  SEQ_sequencer.h
  SEQ_sound.h
  SEQ_time.h
  SEQ_transform.h
  SEQ_utils.h

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
  intern/strip_select.c
  intern/strip_time.c
  intern/strip_time.h
  intern/strip_transform.c
  intern/utils.c
  intern/utils.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
)

if(WITH_AUDASPACE)
  add_definitions(-DWITH_AUDASPACE)

  list(APPEND INC_SYS
    ${AUDASPACE_C_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${AUDASPACE_C_LIBRARIES}
    ${AUDASPACE_PY_LIBRARIES}
  )
endif()

blender_add_lib(bf_sequencer "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# Needed so we can use dna_type_offsets.h.
add_dependencies(bf_sequencer bf_dna)
