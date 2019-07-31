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
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  ../blenfont
  ../blenkernel
  ../blenlib
  ../blentranslation
  ../depsgraph
  ../draw
  ../imbuf
  ../makesdna
  ../makesrna
  ../nodes
  ../render/extern/include
  ../windowmanager
  ../../../intern/guardedalloc

  # for writefile.c: dna_type_offsets.h
  ${CMAKE_BINARY_DIR}/source/blender/makesdna/intern

)

set(INC_SYS
  ${ZLIB_INCLUDE_DIRS}
)

set(SRC
  ${CMAKE_SOURCE_DIR}/release/datafiles/userdef/userdef_default_theme.c
  intern/blend_validate.c
  intern/readblenentry.c
  intern/readfile.c
  intern/undofile.c
  intern/versioning_250.c
  intern/versioning_260.c
  intern/versioning_270.c
  intern/versioning_280.c
  intern/versioning_cycles.c
  intern/versioning_defaults.c
  intern/versioning_dna.c
  intern/versioning_legacy.c
  intern/versioning_userdef.c
  intern/writefile.c

  BLO_blend_defs.h
  BLO_blend_validate.h
  BLO_readfile.h
  BLO_undofile.h
  BLO_writefile.h
  intern/readfile.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
)

if(WITH_BUILDINFO)
  add_definitions(-DWITH_BUILDINFO)
endif()

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_CODEC_FFMPEG)
  add_definitions(-DWITH_FFMPEG)
endif()

if(WITH_ALEMBIC)
  list(APPEND INC
    ../alembic
  )
  add_definitions(-DWITH_ALEMBIC)
endif()

blender_add_lib(bf_blenloader "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# needed so writefile.c can use dna_type_offsets.h
add_dependencies(bf_blenloader bf_dna)
