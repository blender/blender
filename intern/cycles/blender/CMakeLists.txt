
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
	${PYTHON_INCLUDE_DIRS})

SET(LIBRARIES
	render
	bvh
	device
	kernel
	util
	subd
	${Boost_LIBRARIES}
	${OPENGL_LIBRARIES}
	${OPENIMAGEIO_LIBRARY}
	${PYTHON_LIBRARIES}
	${GLUT_LIBRARIES}
	${GLEW_LIBRARIES}
	${BLENDER_LIBRARIES})

IF(WITH_OSL)
	LIST(APPEND LIBRARIES kernel_osl ${OSL_LIBRARIES})
ENDIF(WITH_OSL)

IF(WITH_PARTIO)
	LIST(APPEND LIBRARIES ${PARTIO_LIBRARIES})
ENDIF(WITH_PARTIO)

IF(WITH_OPENCL)
	LIST(APPEND LIBRARIES ${OPENCL_LIBRARIES})
ENDIF(WITH_OPENCL)

SET(CMAKE_MODULE_LINKER_FLAGS ${PYTHON_MODULE_FLAGS})

ADD_LIBRARY(cycles_blender MODULE ${sources} ${headers})
TARGET_LINK_LIBRARIES(cycles_blender ${LIBRARIES})

INSTALL(FILES ${addonfiles} DESTINATION ${INSTALL_PATH}/cycles)
INSTALL(TARGETS cycles_blender LIBRARY DESTINATION ${INSTALL_PATH}/cycles)

IF(UNIX AND NOT APPLE)
	SET_TARGET_PROPERTIES(cycles_blender PROPERTIES INSTALL_RPATH $ORIGIN/lib)
ENDIF()

