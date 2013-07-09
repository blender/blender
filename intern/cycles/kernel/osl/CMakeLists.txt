
set(INC
	.
	..
	../svm
	../../render
	../../util
	../../device
)

set(INC_SYS

)

set(SRC
	background.cpp
	bsdf_diffuse_ramp.cpp
	bsdf_phong_ramp.cpp
	emissive.cpp
	osl_bssrdf.cpp
	osl_closures.cpp
	osl_services.cpp
	osl_shader.cpp
)

set(HEADER_SRC
	osl_bssrdf.h
	osl_closures.h
	osl_globals.h
	osl_services.h
	osl_shader.h
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${RTTI_DISABLE_FLAGS}")

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

add_library(cycles_kernel_osl ${SRC} ${HEADER_SRC})

