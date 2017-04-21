
# GTest
if(WITH_GTESTS)

	Include(GTestTesting)

	add_definitions(${GFLAGS_DEFINES})
	add_definitions(${GLOG_DEFINES})
	add_definitions(-DBLENDER_GFLAGS_NAMESPACE=${GFLAGS_NAMESPACE})

	# Otherwise we get warnings here that we cant fix in external projects
	remove_strict_flags()

	add_subdirectory(testing)
	add_subdirectory(blenlib)
	add_subdirectory(guardedalloc)
	add_subdirectory(bmesh)
	if(WITH_ALEMBIC)
		add_subdirectory(alembic)
	endif()
endif()
