
set(INC
	.
	../device
	../kernel
	../kernel/svm
	../kernel/osl
	../bvh
	../util
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

set(SRC
	attribute.cpp
	background.cpp
	bake.cpp
	blackbody.cpp
	buffers.cpp
	camera.cpp
	film.cpp
	graph.cpp
	image.cpp
	integrator.cpp
	light.cpp
	mesh.cpp
	mesh_displace.cpp
	nodes.cpp
	object.cpp
	osl.cpp
	particles.cpp
	curves.cpp
	scene.cpp
	session.cpp
	shader.cpp
	sky_model.cpp
	sobol.cpp
	svm.cpp
	tables.cpp
	tile.cpp
)

set(SRC_HEADERS
	attribute.h
	bake.h
	background.h
	blackbody.h
	buffers.h
	camera.h
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
	sky_model.h
	sky_model_data.h
	sobol.h
	svm.h
	tables.h
	tile.h
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${RTTI_DISABLE_FLAGS}")

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

add_library(cycles_render ${SRC} ${SRC_HEADERS})

