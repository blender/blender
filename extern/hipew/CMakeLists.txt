# SPDX-FileCopyrightText: 2021 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  include
)

set(INC_SYS

)

set(SRC
  src/hipew.c
  include/hipew.h
)

set(LIB
)

if(HIPRT_INCLUDE_DIR)
  list(APPEND INC_SYS
    ${HIPRT_INCLUDE_DIR}
  )

  list(APPEND SRC
    src/hiprtew.cc
    include/hiprtew.h
  )
endif()

blender_add_lib(extern_hipew "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
