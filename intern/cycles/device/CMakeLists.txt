
include_directories(
	.
	../kernel
	../kernel/svm
	../kernel/osl
	../util
	../render
	${OPENGL_INCLUDE_DIR}
	${GLEW_INCLUDE_PATH})

set(sources
	device.cpp
	device_cpu.cpp
	device_cuda.cpp
	device_multi.cpp
	device_network.cpp
	device_opencl.cpp)

set(headers
	device.h
	device_intern.h
	device_network.h)

add_library(cycles_device ${sources} ${headers})

