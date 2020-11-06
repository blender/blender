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
  ../blenkernel
  ../blenlib
  ../blenloader
  ../blentranslation
  ../depsgraph
  ../imbuf
  ../makesdna
  ../makesrna
  ../render
  ../blenfont
  ../windowmanager
  ../../../intern/atomic
  ../../../intern/guardedalloc

  # dna_type_offsets.h
  ${CMAKE_CURRENT_BINARY_DIR}/../makesdna/intern
)

set(INC_SYS

)

set(SRC
  SEQ_sequencer.h

  intern/effects.c
  intern/image_cache.c
  intern/modifier.c
  intern/prefetch.c
  intern/proxy.c
  intern/proxy.h
  intern/render.c
  intern/render.h
  intern/sequencer.c
  intern/sequencer.h
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
