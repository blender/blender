# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
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
  script_edit.c
  script_ops.c
  space_script.c

  script_intern.h
)

set(LIB
  PRIVATE bf::dna
)

if(WITH_PYTHON)
  list(APPEND INC
    ../../python
  )
  add_definitions(-DWITH_PYTHON)
endif()


blender_add_lib(bf_editor_space_script "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

