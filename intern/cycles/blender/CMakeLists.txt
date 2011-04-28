
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
	cycles_render
	cycles_bvh
	cycles_device
	cycles_kernel
	cycles_util
	cycles_subd
	${Boost_LIBRARIES}
	${OPENGL_LIBRARIES}
	${OPENIMAGEIO_LIBRARY}
	${PYTHON_LIBRARIES}
	${GLUT_LIBRARIES}
	${GLEW_LIBRARIES}
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

SET(CMAKE_MODULE_LINKER_FLAGS ${PYTHON_MODULE_FLAGS})

ADD_LIBRARY(cycles_blender MODULE ${sources} ${headers})
TARGET_LINK_LIBRARIES(cycles_blender ${LIBRARIES})

INSTALL(FILES ${addonfiles} DESTINATION ${CYCLES_INSTALL_PATH}/cycles)
INSTALL(TARGETS cycles_blender LIBRARY DESTINATION ${CYCLES_INSTALL_PATH}/cycles)

IF(UNIX AND NOT APPLE)
	SET_TARGET_PROPERTIES(cycles_blender PROPERTIES INSTALL_RPATH $ORIGIN/lib)
ENDIF()

