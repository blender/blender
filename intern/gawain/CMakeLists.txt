
set(INC
	gawain
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

set(SRC
	src/gwn_attr_binding.c
	src/gwn_batch.c
	src/gwn_element.c
	src/gwn_buffer_id.cpp
	src/gwn_immediate.c
	src/gwn_imm_util.c
	src/gwn_primitive.c
	src/gwn_shader_interface.c
	src/gwn_vertex_array_id.cpp
	src/gwn_vertex_buffer.c
	src/gwn_vertex_format.c

	gawain/gwn_attr_binding.h
	gawain/gwn_attr_binding_private.h
	gawain/gwn_batch.h
	gawain/gwn_batch_private.h
	gawain/gwn_buffer_id.h
	gawain/gwn_common.h
	gawain/gwn_element.h
	gawain/gwn_imm_util.h
	gawain/gwn_immediate.h
	gawain/gwn_primitive.h
	gawain/gwn_primitive_private.h
	gawain/gwn_shader_interface.h
	gawain/gwn_vertex_array_id.h
	gawain/gwn_vertex_buffer.h
	gawain/gwn_vertex_format.h
	gawain/gwn_vertex_format_private.h
)

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_intern_gawain "${SRC}" "${INC}" "${INC_SYS}")
