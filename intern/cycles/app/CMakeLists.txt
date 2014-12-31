
set(INC
	.
	../device
	../kernel
	../kernel/svm
	../bvh
	../util
	../render
	../subd
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
	cycles_util
	${BLENDER_GL_LIBRARIES}
	${CYCLES_APP_GLEW_LIBRARY}
	${PNG_LIBRARIES}
	${JPEG_LIBRARIES}
	${ZLIB_LIBRARIES}
	${TIFF_LIBRARY}
	${PTHREADS_LIBRARIES}
	extern_clew
	extern_cuew
)

if(WITH_CYCLES_OSL)
	list(APPEND LIBRARIES cycles_kernel_osl)
endif()

if(CYCLES_STANDALONE_REPOSITORY)
	if(WITH_CYCLES_LOGGING)
		list(APPEND LIBRARIES
			${GLOG_LIBRARIES}
			${GFLAGS_LIBRARIES}
		)
	endif()
else()
	list(APPEND LIBRARIES bf_intern_glew_mx)
	list(APPEND LIBRARIES extern_glog)
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
		target_link_libraries_decoupled(${target} OSL_LIBRARIES)
		if(MSVC)
			target_link_libraries_debug(${target} "${LLVM_LIBRARIES_DEBUG}")
			target_link_libraries_optimized(${target} "${LLVM_LIBRARIES}")
		else()
			target_link_libraries(${target} ${LLVM_LIBRARIES})
		endif()
	endif()
	target_link_libraries_decoupled(${target} OPENIMAGEIO_LIBRARIES)
	target_link_libraries_decoupled(${target} OPENEXR_LIBRARIES)
	target_link_libraries(
		${target}
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
