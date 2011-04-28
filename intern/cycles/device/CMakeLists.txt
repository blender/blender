
INCLUDE_DIRECTORIES(
	.
	../kernel
	../kernel/svm
	../kernel/osl
	../util
	../render
	${OPENGL_INCLUDE_DIR}
	${GLEW_INCLUDE_DIR})

SET(sources
	device.cpp
	device_cpu.cpp
	device_cuda.cpp
	device_multi.cpp
	device_network.cpp
	device_opencl.cpp)

SET(headers
	device.h
	device_intern.h
	device_network.h)

ADD_LIBRARY(cycles_device ${sources} ${headers})

