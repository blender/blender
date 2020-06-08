
# GTest
if(WITH_GTESTS)

  include(GTestTesting)

  add_definitions(${GFLAGS_DEFINES})
  add_definitions(${GLOG_DEFINES})
  add_definitions(-DBLENDER_GFLAGS_NAMESPACE=${GFLAGS_NAMESPACE})

  # Otherwise we get warnings here that we cant fix in external projects
  remove_strict_flags()

  add_subdirectory(testing)
  add_subdirectory(blenkernel)
  add_subdirectory(blenlib)
  add_subdirectory(blenloader)
  add_subdirectory(guardedalloc)
  add_subdirectory(bmesh)
  add_subdirectory(functions)
  if(WITH_CODEC_FFMPEG)
    add_subdirectory(ffmpeg)
  endif()
  if(WITH_ALEMBIC)
    add_subdirectory(alembic)
  endif()
  if(WITH_USD)
    add_subdirectory(usd)
  endif()
endif()
