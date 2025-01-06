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
  include/hipew6.h
  include/hipew5.h
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
    src/util.h
  )
endif()


blender_add_lib(extern_hipew "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

find_package(HIP 5.5.0)
if(HIP_VERSION_MAJOR EQUAL 5)
  target_compile_definitions(extern_hipew PUBLIC WITH_HIP_SDK_5)
endif()
