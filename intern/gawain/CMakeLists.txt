
set(INC
	gawain
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

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
	gawain/attrib_binding_private.h
	gawain/batch.h
	gawain/buffer_id.h
	gawain/common.h
	gawain/element.h
	gawain/imm_util.h
	gawain/immediate.h
	gawain/primitive.h
	gawain/primitive_private.h
	gawain/shader_interface.h
	gawain/vertex_buffer.h
	gawain/vertex_format.h
	gawain/vertex_format_private.h
)

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_intern_gawain "${SRC}" "${INC}" "${INC_SYS}")
