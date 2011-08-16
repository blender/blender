
set(sources
	blender_camera.cpp
	blender_mesh.cpp
	blender_object.cpp
	blender_python.cpp
	blender_session.cpp
	blender_shader.cpp
	blender_sync.cpp

	blender_sync.h
	blender_session.h
	blender_util.h)

set(addonfiles
	addon/__init__.py
	addon/engine.py 
	addon/enums.py
	addon/properties.py
	addon/ui.py
	addon/xml.py)

include_directories(
	../render
	../device
	../kernel
	../kernel/svm
	../util
	../subd
	${BLENDER_INCLUDE_DIRS}
	${PYTHON_INCLUDE_DIRS}
	${GLEW_INCLUDE_PATH})

set(LIBRARIES
	cycles_render
	cycles_bvh
	cycles_device
	cycles_kernel
	cycles_util
	cycles_subd)

if(WITH_CYCLES_OSL)
	list(APPEND LIBRARIES cycles_kernel_osl ${OSL_LIBRARIES})
endif()

if(WITH_CYCLES_PARTIO)
	list(APPEND LIBRARIES ${PARTIO_LIBRARIES})
endif()

if(WITH_CYCLES_OPENCL)
	list(APPEND LIBRARIES ${OPENCL_LIBRARIES})
endif()

blender_add_lib(bf_intern_cycles "${sources}" "" "")

add_dependencies(bf_intern_cycles bf_rna)
target_link_libraries(bf_intern_cycles ${LIBRARIES})

delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${addonfiles}" ${CYCLES_INSTALL_PATH})

