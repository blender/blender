# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../../blenkernel
  ../../blenloader
  ../../blentranslation
  ../../gpu
  ../../makesrna
  ../../windowmanager

)

set(INC_SYS
)

set(SRC
  script_edit.cc
  script_ops.cc
  space_script.cc

  script_intern.h
)

set(LIB
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
)

if(WITH_PYTHON)
  list(APPEND INC
    ../../python
  )
  add_definitions(-DWITH_PYTHON)
endif()


blender_add_lib(bf_editor_space_script "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
