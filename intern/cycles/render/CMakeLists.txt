
include_directories(
	.
	../device
	../kernel
	../kernel/svm
	../kernel/osl
	../bvh
	../util
	${GLEW_INCLUDE_PATH})

set(sources
	attribute.cpp
	background.cpp
	buffers.cpp
	camera.cpp
	film.cpp
	film_response.cpp
	filter.cpp
	graph.cpp
	image.cpp
	integrator.cpp
	light.cpp
	mesh.cpp
	mesh_displace.cpp
	nodes.cpp
	object.cpp
	osl.cpp
	scene.cpp
	session.cpp
	shader.cpp
	sobol.cpp
	svm.cpp
	tile.cpp)

set(headers
	attribute.h
	background.h
	buffers.h
	camera.h
	film.h
	film_response.h
	filter.h
	graph.h
	image.h
	integrator.h
	light.h
	mesh.h
	nodes.h
	object.h
	osl.h
	scene.h
	session.h
	shader.h
	sobol.h
	svm.h
	tile.h)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${RTTI_DISABLE_FLAGS}")

add_library(cycles_render ${sources} ${headers})

