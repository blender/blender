
if(WITH_GTESTS)
  # Otherwise we get warnings here that we cant fix in external projects
  remove_strict_flags()

  # Build common test executable used by most tests
  add_subdirectory(runner)

  # Build utility library used by test executables
  add_subdirectory(testing)
endif()
