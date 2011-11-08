
include_directories(
	.
	../device
	../kernel
	../kernel/svm
	../bvh
	../util
	../render
	../subd)

set(LIBRARIES
	cycles_device
	cycles_kernel
	cycles_render
	cycles_bvh
	cycles_subd
	cycles_util
	${BOOST_LIBRARIES}
	${OPENGL_LIBRARIES}
	${CYCLES_GLEW_LIBRARY}
	${OPENIMAGEIO_LIBRARIES})

link_directories(${OPENIMAGEIO_LIBPATH} ${BOOST_LIBPATH})

if(WITH_CYCLES_TEST)
	list(APPEND LIBRARIES ${GLUT_LIBRARIES})
endif()

if(WITH_CYCLES_OSL)
	list(APPEND LIBRARIES cycles_kernel_osl ${OSL_LIBRARIES})
endif()

if(WITH_CYCLES_PARTIO)
	list(APPEND LIBRARIES ${PARTIO_LIBRARIES})
endif()

if(WITH_CYCLES_TEST)
	add_executable(cycles_test cycles_test.cpp cycles_xml.cpp cycles_xml.h)
	target_link_libraries(cycles_test ${LIBRARIES})

	if(UNIX AND NOT APPLE)
		set_target_properties(cycles_test PROPERTIES INSTALL_RPATH $ORIGIN/lib)
	endif()
endif()

if(WITH_CYCLES_NETWORK)
	add_executable(cycles_server cycles_server.cpp)
	target_link_libraries(cycles_server ${LIBRARIES})

	if(UNIX AND NOT APPLE)
		set_target_properties(cycles_server PROPERTIES INSTALL_RPATH $ORIGIN/lib)
	endif()
endif()

