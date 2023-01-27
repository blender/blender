# SPDX-License-Identifier: GPL-2.0-or-later

if(WIN32)

  if(MSVC14) # vs2015 has timespec
    set(PTHREAD_CPPFLAGS "/I. /DHAVE_CONFIG_H /D_TIMESPEC_DEFINED ")
  else() # everything before doesn't
    set(PTHREAD_CPPFLAGS "/I. /DHAVE_CONFIG_H ")
  endif()

  set(PTHREADS_BUILD cd ${BUILD_DIR}/pthreads/src/external_pthreads/ && cd && nmake VC-static /e CPPFLAGS=${PTHREAD_CPPFLAGS})

  ExternalProject_Add(external_pthreads
    URL file://${PACKAGE_DIR}/${PTHREADS_FILE}
    DOWNLOAD_DIR ${DOWNLOAD_DIR}
    URL_HASH ${PTHREADS_HASH_TYPE}=${PTHREADS_HASH}
    PREFIX ${BUILD_DIR}/pthreads
    CONFIGURE_COMMAND echo .
    PATCH_COMMAND COMMAND ${PATCH_CMD} -p 1 -d ${BUILD_DIR}/pthreads/src/external_pthreads < ${PATCH_DIR}/pthreads.diff
    BUILD_COMMAND ${PTHREADS_BUILD}
    INSTALL_COMMAND COMMAND
      ${CMAKE_COMMAND} -E copy ${BUILD_DIR}/pthreads/src/external_pthreads/libpthreadVC3${LIBEXT} ${LIBDIR}/pthreads/lib/pthreadVC3${LIBEXT} &&
      ${CMAKE_COMMAND} -E copy ${BUILD_DIR}/pthreads/src/external_pthreads/pthread.h ${LIBDIR}/pthreads/inc/pthread.h &&
      ${CMAKE_COMMAND} -E copy ${BUILD_DIR}/pthreads/src/external_pthreads/sched.h ${LIBDIR}/pthreads/inc/sched.h &&
      ${CMAKE_COMMAND} -E copy ${BUILD_DIR}/pthreads/src/external_pthreads/semaphore.h ${LIBDIR}/pthreads/inc/semaphore.h &&
      ${CMAKE_COMMAND} -E copy ${BUILD_DIR}/pthreads/src/external_pthreads/_ptw32.h ${LIBDIR}/pthreads/inc/_ptw32.h
    INSTALL_DIR ${LIBDIR}/pthreads
  )

  if(BUILD_MODE STREQUAL Release)
    ExternalProject_Add_Step(external_pthreads after_install
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/pthreads/inc/ ${HARVEST_TARGET}/pthreads/include/
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/pthreads/lib/ ${HARVEST_TARGET}/pthreads/lib
      DEPENDEES install
    )
  endif()
endif()
