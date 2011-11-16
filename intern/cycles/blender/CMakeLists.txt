
set(INC
	../render
	../device
	../kernel
	../kernel/svm
	../util
	../subd
)

set(INC_SYS
	${BLENDER_INCLUDE_DIRS}
	${PYTHON_INCLUDE_DIRS}
	${GLEW_INCLUDE_PATH}
)


set(SRC
	blender_camera.cpp
	blender_mesh.cpp
	blender_object.cpp
	blender_python.cpp
	blender_session.cpp
	blender_shader.cpp
	blender_sync.cpp

	blender_sync.h
	blender_session.h
	blender_util.h
)

set(ADDON_FILES
	addon/__init__.py
	addon/engine.py 
	addon/enums.py
	addon/presets.py
	addon/properties.py
	addon/ui.py
	addon/xml.py
)

blender_add_lib(bf_intern_cycles "${SRC}" "${INC}" "${INC_SYS}")

add_dependencies(bf_intern_cycles bf_rna)

delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${ADDON_FILES}" ${CYCLES_INSTALL_PATH})

