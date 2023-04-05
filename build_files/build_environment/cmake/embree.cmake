# SPDX-License-Identifier: GPL-2.0-or-later

# Note the utility apps may use png/tiff/gif system libraries, but the
# library itself does not depend on them, so should give no problems.

set(EMBREE_EXTRA_ARGS
  -DEMBREE_ISPC_SUPPORT=OFF
  -DEMBREE_TUTORIALS=OFF
  -DEMBREE_STATIC_LIB=OFF
  -DEMBREE_RAY_MASK=ON
  -DEMBREE_FILTER_FUNCTION=ON
  -DEMBREE_BACKFACE_CULLING=OFF
  -DEMBREE_BACKFACE_CULLING_CURVES=ON
  -DEMBREE_BACKFACE_CULLING_SPHERES=ON
  -DEMBREE_TASKING_SYSTEM=TBB
  -DEMBREE_TBB_ROOT=${LIBDIR}/tbb
  -DTBB_ROOT=${LIBDIR}/tbb
)

if(WIN32)
  set(EMBREE_EXTRA_ARGS
    ${EMBREE_EXTRA_ARGS}
    -DCMAKE_DEBUG_POSTFIX=_d
  )
endif()

if(NOT BLENDER_PLATFORM_ARM)
  set(EMBREE_EXTRA_ARGS
    ${EMBREE_EXTRA_ARGS}
    -DEMBREE_MAX_ISA=AVX2
  )
endif()

if(TBB_STATIC_LIBRARY)
  set(EMBREE_EXTRA_ARGS
    ${EMBREE_EXTRA_ARGS}
    -DEMBREE_TBB_COMPONENT=tbb_static
  )
endif()

ExternalProject_Add(external_embree
  URL file://${PACKAGE_DIR}/${EMBREE_FILE}
  DOWNLOAD_DIR ${DOWNLOAD_DIR}
  URL_HASH ${EMBREE_HASH_TYPE}=${EMBREE_HASH}
  PREFIX ${BUILD_DIR}/embree
  PATCH_COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/embree/src/external_embree < ${PATCH_DIR}/embree.diff
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${LIBDIR}/embree ${DEFAULT_CMAKE_FLAGS} ${EMBREE_EXTRA_ARGS}
  INSTALL_DIR ${LIBDIR}/embree
)

add_dependencies(
  external_embree
  external_tbb
)

if(WIN32)
  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_embree after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/embree/include ${HARVEST_TARGET}/embree/include
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/embree/lib ${HARVEST_TARGET}/embree/lib
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/embree/share ${HARVEST_TARGET}/embree/share
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/embree/bin/embree4.dll ${HARVEST_TARGET}/embree/bin/embree4.dll
      DEPENDEES install
    )
  else()
    ExternalProject_Add_Step(external_embree after_install
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/embree/bin/embree4_d.dll ${HARVEST_TARGET}/embree/bin/embree4_d.dll
      COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/embree/lib/embree4_d.lib ${HARVEST_TARGET}/embree/lib/embree4_d.lib
      DEPENDEES install
    )
  endif()
endif()
