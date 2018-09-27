
set(INC
	..
)
set(INC_SYS
)

# NOTE: LIBRARIES contains all the libraries which are common
# across release and debug build types, stored in a linking order.
set(LIBRARIES
	cycles_device
	cycles_kernel
	cycles_render
	cycles_bvh
	cycles_subd
	cycles_graph
	cycles_util
	${BLENDER_GL_LIBRARIES}
	${CYCLES_APP_GLEW_LIBRARY}
	${PNG_LIBRARIES}
	${JPEG_LIBRARIES}
	${ZLIB_LIBRARIES}
	${TIFF_LIBRARY}
	${PTHREADS_LIBRARIES}
	extern_clew
)

if(WITH_CUDA_DYNLOAD)
	list(APPEND LIBRARIES extern_cuew)
else()
	list(APPEND LIBRARIES ${CUDA_CUDA_LIBRARY})
endif()

if(WITH_CYCLES_OSL)
	list(APPEND LIBRARIES cycles_kernel_osl)
endif()

if(NOT CYCLES_STANDALONE_REPOSITORY)
	list(APPEND LIBRARIES bf_intern_glew_mx bf_intern_guardedalloc)
endif()

if(WITH_CYCLES_LOGGING)
	list(APPEND LIBRARIES
		${GLOG_LIBRARIES}
		${GFLAGS_LIBRARIES}
	)
endif()

if(WITH_CYCLES_STANDALONE AND WITH_CYCLES_STANDALONE_GUI)
	list(APPEND LIBRARIES ${GLUT_LIBRARIES})
endif()

# Common configuration.

link_directories(${OPENIMAGEIO_LIBPATH}
                 ${BOOST_LIBPATH}
                 ${PNG_LIBPATH}
                 ${JPEG_LIBPATH}
                 ${ZLIB_LIBPATH}
                 ${TIFF_LIBPATH}
                 ${OPENEXR_LIBPATH})

add_definitions(${GL_DEFINITIONS})

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

# Make sure given target is linked against proper libraries
# which varies across debug and release build types.
#
# This will also make sure dependencies of that libraries
# are sent to the linker after them.
#
# TODO(sergey): Think of a better place for this?
macro(cycles_target_link_libraries target)
	target_link_libraries(${target} ${LIBRARIES})
	if(WITH_CYCLES_OSL)
		target_link_libraries(${target} ${OSL_LIBRARIES} ${LLVM_LIBRARIES})
	endif()
	if(WITH_CYCLES_OPENSUBDIV)
		target_link_libraries(${target} ${OPENSUBDIV_LIBRARIES})
	endif()
	if(WITH_OPENCOLORIO)
		link_directories(${OPENCOLORIO_LIBPATH})
		target_link_libraries(${target} ${OPENCOLORIO_LIBRARIES})
	endif()
	target_link_libraries(
		${target}
		${OPENIMAGEIO_LIBRARIES}
		${OPENEXR_LIBRARIES}
		${PUGIXML_LIBRARIES}
		${BOOST_LIBRARIES}
		${CMAKE_DL_LIBS}
		${PLATFORM_LINKLIBS}
	)
endmacro()

# Application build targets

if(WITH_CYCLES_STANDALONE)
	set(SRC
		cycles_standalone.cpp
		cycles_xml.cpp
		cycles_xml.h
	)
	add_executable(cycles ${SRC})
	cycles_target_link_libraries(cycles)

	if(UNIX AND NOT APPLE)
		set_target_properties(cycles PROPERTIES INSTALL_RPATH $ORIGIN/lib)
	endif()
	unset(SRC)
endif()

if(WITH_CYCLES_NETWORK)
	set(SRC
		cycles_server.cpp
	)
	add_executable(cycles_server ${SRC})
	cycles_target_link_libraries(cycles_server)

	if(UNIX AND NOT APPLE)
		set_target_properties(cycles_server PROPERTIES INSTALL_RPATH $ORIGIN/lib)
	endif()
	unset(SRC)
endif()

if(WITH_CYCLES_CUBIN_COMPILER)
	# 32 bit windows is special, nvrtc is not supported on x86, so even
	# though we are building 32 bit blender a 64 bit cubin_cc will have
	# to be build to compile the cubins.
	if(MSVC AND NOT CMAKE_CL_64)
		message("Building with CUDA not supported on 32 bit, skipped")
		set(WITH_CYCLES_CUDA_BINARIES OFF CACHE BOOL "" FORCE)
	else()
		set(SRC
			cycles_cubin_cc.cpp
		)
		set(INC
			../../../extern/cuew/include
		)
		add_executable(cycles_cubin_cc ${SRC})
		include_directories(${INC})
		target_link_libraries(cycles_cubin_cc
			extern_cuew
			${OPENIMAGEIO_LIBRARIES}
			${OPENEXR_LIBRARIES}
			${PUGIXML_LIBRARIES}
			${BOOST_LIBRARIES}
			${PLATFORM_LINKLIBS}
		)
		if(NOT CYCLES_STANDALONE_REPOSITORY)
			target_link_libraries(cycles_cubin_cc bf_intern_guardedalloc)
		endif()
		unset(SRC)
		unset(INC)
	endif()
endif()
