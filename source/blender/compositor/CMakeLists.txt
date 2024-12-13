# SPDX-FileCopyrightText: 2011 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

add_subdirectory(realtime_compositor)

set(INC
  .
  intern
  realtime_compositor
  ../blenkernel
  ../blentranslation
  ../imbuf
  ../makesrna
  ../nodes
  ../windowmanager
  ../nodes/composite
  ../nodes/intern
  ../render
  ../render/intern

  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS

)

set(SRC
  COM_compositor.hh

  intern/COM_compositor.cc
)

set(LIB
  bf_blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  bf_realtime_compositor
  PRIVATE bf::intern::atomic
)

if(WITH_TBB)
  if(WIN32)
    # TBB includes Windows.h which will define min/max macros
    # that will collide with the STL versions.
    add_definitions(-DNOMINMAX)
  endif()
endif()

if(WITH_OPENIMAGEDENOISE)
  add_definitions(-DWITH_OPENIMAGEDENOISE)
  add_definitions(-DOIDN_STATIC_LIB)
  list(APPEND INC_SYS
    ${OPENIMAGEDENOISE_INCLUDE_DIRS}
    ${TBB_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${OPENIMAGEDENOISE_LIBRARIES}
    ${TBB_LIBRARIES}
  )
endif()

if(WITH_FFTW3)
  list(APPEND INC_SYS
    ${FFTW3_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${FFTW3_LIBRARIES}
  )
  add_definitions(-DWITH_FFTW3)
endif()

blender_add_lib(bf_compositor "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

blender_set_target_unity_build(bf_compositor 10)

if(CXX_WARN_NO_SUGGEST_OVERRIDE)
  target_compile_options(bf_compositor PRIVATE "-Wsuggest-override")
endif()

# RNA_prototypes.hh
add_dependencies(bf_compositor bf_rna)
