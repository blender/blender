
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

if(APPLE)
  # Disable allocation warning on macOS prior to 10.14: the OSLRenderServices
  # contains member which is 64 bytes aligned (cache inside of OIIO's
  # unordered_map_concurrent). This is not something what the SDK supportsm, but
  # since we take care of allocations ourselves is is OK to ignore the
  # diagnostic message.
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -faligned-allocation")
endif()

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

cycles_add_library(cycles_kernel_osl "${LIB}" ${SRC} ${HEADER_SRC})
