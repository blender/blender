# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2013 Blender Foundation. All rights reserved.

set(INC
  include
)

set(INC_SYS

)

if(UNIX)
  list(APPEND INC_SYS
    ${X11_X11_INCLUDE_PATH}
  )
endif()

set(SRC
  src/glew.c

  include/GL/eglew.h
  include/GL/glesew.h
  include/GL/glew.h
  include/GL/glxew.h
  include/GL/wglew.h
)

set(LIB
)

add_definitions(${GL_DEFINITIONS})

blender_add_lib(extern_glew_es "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
