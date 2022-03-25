# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2014 Blender Foundation. All rights reserved.

set(INC
  .
)

set(INC_SYS
  ${GLEW_INCLUDE_PATH}
)

set(SRC
  intern/glew-mx.c

  glew-mx.h
  intern/gl-deprecated.h
  intern/symbol-binding.h
)

set(LIB
)

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_intern_glew_mx "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
