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
  ../common
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../bmesh
  ../../depsgraph
  ../../editors/include
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/clog
  ../../../../intern/guardedalloc
  ../../../../intern/utfconv
)

set(INC_SYS
)

set(SRC
  intern/gpencil_io_base.cc
  intern/gpencil_io_capi.cc
  intern/gpencil_io_import_base.cc
  intern/gpencil_io_import_svg.cc

  # This line must be removed if NanoSVG is moved to extern
  nanosvg/nanosvg.h

  gpencil_io.h
  intern/gpencil_io_base.hh
  intern/gpencil_io_export_base.hh
  intern/gpencil_io_import_base.hh
  intern/gpencil_io_import_svg.hh
)

set(LIB
  bf_blenkernel
  bf_blenlib
  bf_io_common
)

if(WITH_PUGIXML)
  list(APPEND SRC
    intern/gpencil_io_export_svg.cc

    intern/gpencil_io_export_svg.hh
  )
  list(APPEND INC_SYS
    ${PUGIXML_INCLUDE_DIR}
  )
  list(APPEND LIB
    ${PUGIXML_LIBRARIES}
  )
  add_definitions(-DWITH_PUGIXML)
endif()

if(WITH_HARU)
  list(APPEND SRC
    intern/gpencil_io_export_pdf.cc

    intern/gpencil_io_export_pdf.hh
  )
  list(APPEND INC_SYS
    ${HARU_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${HARU_LIBRARIES}
  )
  add_definitions(-DWITH_HARU)
endif()


list(APPEND LIB
  ${BOOST_LIBRARIES}
)

blender_add_lib(bf_gpencil "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
