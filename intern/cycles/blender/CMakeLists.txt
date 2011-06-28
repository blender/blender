
SET(sources
	blender_camera.cpp
	blender_mesh.cpp
	blender_object.cpp
	blender_python.cpp
	blender_session.cpp
	blender_shader.cpp
	blender_sync.cpp)

SET(headers
	blender_sync.h
	blender_session.h
	blender_util.h)

SET(addonfiles
	addon/__init__.py
	addon/engine.py 
	addon/enums.py
	addon/properties.py
	addon/ui.py
	addon/xml.py)

INCLUDE_DIRECTORIES(
	../render
	../device
	../kernel
	../kernel/svm
	../util
	../subd
	${BLENDER_INCLUDE_DIRS}
	${PYTHON_INCLUDE_DIRS}
	${GLEW_INCLUDE_PATH})

SET(LIBRARIES
	cycles_render
	cycles_bvh
	cycles_device
	cycles_kernel
	cycles_util
	cycles_subd
	${Boost_LIBRARIES}
	${OPENGL_LIBRARIES}
	${OPENIMAGEIO_LIBRARY}
	${GLUT_LIBRARIES}
	${CYCLES_GLEW_LIBRARY}
	${BLENDER_LIBRARIES})

IF(WITH_CYCLES_OSL)
	LIST(APPEND LIBRARIES cycles_kernel_osl ${OSL_LIBRARIES})
ENDIF()

IF(WITH_CYCLES_PARTIO)
	LIST(APPEND LIBRARIES ${PARTIO_LIBRARIES})
ENDIF()

IF(WITH_CYCLES_OPENCL)
	LIST(APPEND LIBRARIES ${OPENCL_LIBRARIES})
ENDIF()

LINK_DIRECTORIES(${PYTHON_LIBPATH})
SET(CMAKE_MODULE_LINKER_FLAGS ${PYTHON_MODULE_FLAGS})

ADD_LIBRARY(cycles_blender MODULE ${sources} ${headers})
ADD_DEPENDENCIES(cycles_blender bf_rna)

IF(WIN32)
	TARGET_LINK_LIBRARIES(cycles_blender ${PYTHON_LINKFLAGS})
	TARGET_LINK_LIBRARIES(cycles_blender debug ${PYTHON_LIBRARIES}_d)
	TARGET_LINK_LIBRARIES(cycles_blender optimized ${PYTHON_LIBRARIES})

	SET_TARGET_PROPERTIES(cycles_blender PROPERTIES PREFIX "lib")
	SET_TARGET_PROPERTIES(cycles_blender PROPERTIES SUFFIX ".pyd")
ENDIF()

TARGET_LINK_LIBRARIES(cycles_blender ${LIBRARIES})

INSTALL(FILES ${addonfiles} DESTINATION ${CYCLES_INSTALL_PATH}/cycles)
INSTALL(TARGETS cycles_blender LIBRARY DESTINATION ${CYCLES_INSTALL_PATH}/cycles)

# Install Dynamic Libraries

IF(WIN32)
	FILE(GLOB OIIO_DLLS "${CYCLES_OIIO}/bin/*.dll")
	FILE(GLOB BOOST_DLLS "${CYCLES_BOOST}/lib/*.dll")
	INSTALL(FILES ${OIIO_DLLS} ${BOOST_DLLS}
		DESTINATION ${CYCLES_INSTALL_PATH}/cycles)
ENDIF()

IF(UNIX)
	# copy libraries to cycles lib directory
	SET(install_libs
		${OPENIMAGEIO_LIBRARY}
		${Boost_LIBRARIES}
		${OSL_LIBRARIES}
		${PARTIO_LIBRARIES})
	
	LIST(REMOVE_ITEM install_libs optimized)
	LIST(REMOVE_ITEM install_libs debug)

	INSTALL(FILES ${install_libs}
		DESTINATION ${CYCLES_INSTALL_PATH}/cycles/lib)
	
	IF(NOT APPLE)
		# set path to look for dynamic libs
		SET_TARGET_PROPERTIES(cycles_blender PROPERTIES INSTALL_RPATH $ORIGIN/lib)
	ELSE()
		# modify our libs to looks for dynamic libs in cycles lib directory
		SET(install_name_command "install_name_tool")

		FOREACH(lib ${install_libs})
			GET_FILENAME_COMPONENT(libname ${lib} NAME)
			SET(install_name_command " ${install_name_command} -change ${lib} @loader_path/lib/${libname}")
			SET(install_name_command " ${install_name_command} -change ${libname} @loader_path/lib/${libname}")
		ENDFOREACH()

		INSTALL(
			CODE
			"
			EXECUTE_PROCESS(COMMAND ${install_name_command} ${CYCLES_INSTALL_PATH}/cycles/libcycles_blender.so)
			"
		)
	ELSE()
	ENDIF()
ENDIF()


