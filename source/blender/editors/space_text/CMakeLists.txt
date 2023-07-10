# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenfont
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../blentranslation
  ../../gpu
  ../../makesrna
  ../../windowmanager
  ../../../../intern/guardedalloc

)

set(INC_SYS
)

set(SRC
  space_text.cc
  text_autocomplete.cc
  text_draw.cc
  text_format.cc
  text_format_osl.cc
  text_format_pov.cc
  text_format_pov_ini.cc
  text_format_py.cc
  text_header.cc
  text_ops.cc
  text_undo.cc

  text_format.hh
  text_intern.hh
)

set(LIB
  bf_blenkernel
  bf_blenlib
  PRIVATE bf::dna
)


if(WITH_PYTHON)
  list(APPEND INC
    ../../python
  )
  add_definitions(-DWITH_PYTHON)
endif()


blender_add_lib(bf_editor_space_text "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

