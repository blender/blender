
set(INC
	..
	../../glew-mx
)

set(INC_SYS
	${GLEW_INCLUDE_DIR}
)

set(SRC
	attribute.cpp
	background.cpp
	bake.cpp
	buffers.cpp
	camera.cpp
	constant_fold.cpp
	film.cpp
	graph.cpp
	image.cpp
	integrator.cpp
	light.cpp
	mesh.cpp
	mesh_displace.cpp
	mesh_subdivision.cpp
	mesh_volume.cpp
	nodes.cpp
	object.cpp
	osl.cpp
	particles.cpp
	curves.cpp
	scene.cpp
	session.cpp
	shader.cpp
	sobol.cpp
	stats.cpp
	svm.cpp
	tables.cpp
	tile.cpp
)

set(SRC_HEADERS
	attribute.h
	bake.h
	background.h
	buffers.h
	camera.h
	constant_fold.h
	film.h
	graph.h
	image.h
	integrator.h
	light.h
	mesh.h
	nodes.h
	object.h
	osl.h
	particles.h
	curves.h
	scene.h
	session.h
	shader.h
	sobol.h
	stats.h
	svm.h
	tables.h
	tile.h
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${RTTI_DISABLE_FLAGS}")

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

add_definitions(${GL_DEFINITIONS})

cycles_add_library(cycles_render ${SRC} ${SRC_HEADERS})
