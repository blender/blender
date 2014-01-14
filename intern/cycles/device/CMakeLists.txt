
set(INC
	.
	../kernel
	../kernel/svm
	../kernel/osl
	../util
	../render
)

set(INC_SYS
	${OPENGL_INCLUDE_DIR}
	${GLEW_INCLUDE_PATH}
)

set(SRC
	device.cpp
	device_cpu.cpp
	device_cuda.cpp
	device_multi.cpp
	device_opencl.cpp
	device_task.cpp
)

if(WITH_CYCLES_NETWORK)
	list(APPEND SRC
		device_network.cpp
	)
endif()

set(SRC_HEADERS
	device.h
	device_memory.h
	device_intern.h
	device_network.h
	device_task.h
)

add_definitions(-DGLEW_STATIC)

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

add_library(cycles_device ${SRC} ${SRC_HEADERS})
