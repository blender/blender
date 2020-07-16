
if(WITH_GTESTS)
  # Otherwise we get warnings here that we cant fix in external projects
  remove_strict_flags()

  # Build common test runner
  add_subdirectory(runner)

  # Build tests not yet ported to the common runner
  add_subdirectory(testing)
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
endif()
