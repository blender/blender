
set(INC
  ../..
)

set(INC_SYS

)

set(SRC
  background.cpp
  bsdf_diffuse_ramp.cpp
  bsdf_phong_ramp.cpp
  emissive.cpp
  osl_bssrdf.cpp
  osl_closures.cpp
  osl_services.cpp
  osl_shader.cpp
)

set(HEADER_SRC
  osl_closures.h
  osl_globals.h
  osl_services.h
  osl_shader.h
)

set(LIB
  cycles_render

  ${OSL_LIBRARIES}
  ${OPENIMAGEIO_LIBRARIES}
  ${LLVM_LIBRARY}
)

# OSL and LLVM are built without RTTI
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${RTTI_DISABLE_FLAGS}")

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

cycles_add_library(cycles_kernel_osl "${LIB}" ${SRC} ${HEADER_SRC})
