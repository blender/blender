
INCLUDE_DIRECTORIES(. ../device ../kernel ../kernel/svm ../kernel/osl ../bvh ../util)

SET(sources
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

SET(headers
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

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${RTTI_DISABLE_FLAGS}")

ADD_LIBRARY(render ${sources} ${headers})

