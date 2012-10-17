
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
	buffers.cpp
	camera.cpp
	film.cpp
	# film_response.cpp (code unused)
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
	particles.cpp
	scene.cpp
	session.cpp
	shader.cpp
	sobol.cpp
	svm.cpp
	tile.cpp
)

set(SRC_HEADERS
	attribute.h
	background.h
	buffers.h
	camera.h
	film.h
	# film_response.h (code unused)
	filter.h
	graph.h
	image.h
	integrator.h
	light.h
	mesh.h
	nodes.h
	object.h
	osl.h
	particles.h
	scene.h
	session.h
	shader.h
	sobol.h
	svm.h
	tile.h
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${RTTI_DISABLE_FLAGS}")

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

add_library(cycles_render ${SRC} ${SRC_HEADERS})

