
set(INC
	.
	${GLEW_INCLUDE_PATH}
	${OPENGL_INCLUDE_DIR}
)

set(SRC
	util_cache.cpp
	util_cuda.cpp
	util_dynlib.cpp
	util_md5.cpp
	util_memarena.cpp
	util_opencl.cpp
	util_path.cpp
	util_string.cpp
	util_system.cpp
	util_task.cpp
	util_time.cpp
	util_transform.cpp
)

if(WITH_CYCLES_TEST)
	list(APPEND SRC
		util_view.cpp
	)
endif()

set(SRC_HEADERS
	util_algorithm.h
	util_args.h
	util_boundbox.h
	util_cache.h
	util_cuda.h
	util_debug.h
	util_dynlib.h
	util_foreach.h
	util_function.h
	util_hash.h
	util_image.h
	util_list.h
	util_map.h
	util_math.h
	util_md5.h
	util_memarena.h
	util_opencl.h
	util_opengl.h
	util_param.h
	util_path.h
	util_progress.h
	util_set.h
	util_string.h
	util_system.h
	util_task.h
	util_thread.h
	util_time.h
	util_transform.h
	util_types.h
	util_view.h
	util_vector.h
	util_xml.h
)

include_directories(${INC})

add_library(cycles_util ${SRC} ${SRC_HEADERS})
