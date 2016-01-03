if(WITH_LIBMV)
	add_subdirectory(ceres)
endif()

if(WITH_LIBMV OR WITH_GTESTS OR (WITH_CYCLES AND WITH_CYCLES_LOGGING))
	add_subdirectory(gflags)
	add_subdirectory(glog)
endif()
