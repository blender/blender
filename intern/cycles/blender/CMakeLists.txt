
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
	addon/presets.py
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

blender_add_lib(bf_intern_cycles "${sources}" "" "")
add_dependencies(bf_intern_cycles bf_rna)

delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${addonfiles}" ${CYCLES_INSTALL_PATH})

