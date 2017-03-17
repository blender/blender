
set(INC gawain)

set(INC_SYS ${GLEW_INCLUDE_PATH})

set(SRC
	src/attrib_binding.c
	src/batch.c
	src/element.c
	src/buffer_id.cpp
	src/immediate.c
	src/imm_util.c
	src/primitive.c
	src/shader_interface.c
	src/vertex_buffer.c
	src/vertex_format.c

	gawain/attrib_binding.h
	gawain/batch.h
	gawain/buffer_id.h
	gawain/common.h
	gawain/element.h
	gawain/immediate.h
	gawain/imm_util.h
	gawain/primitive.h
	gawain/shader_interface.h
	gawain/vertex_buffer.h
	gawain/vertex_format.h
)

# remove this when we switch to core profile
add_definitions(-DWITH_GL_PROFILE_COMPAT)

blender_add_lib(bf_intern_gawain "${SRC}" "${INC}" "${INC_SYS}")
