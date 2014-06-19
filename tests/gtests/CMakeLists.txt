
# GTest
if(WITH_GTESTS)

	Include(GTestTesting)

	# Otherwise we get warnings here that we cant fix in external projects
	remove_strict_flags()

	add_subdirectory(testing)
	add_subdirectory(blenlib)
	add_subdirectory(guardedalloc)
	add_subdirectory(bmesh)
endif()

