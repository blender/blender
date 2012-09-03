
set(INC
	.
	..
	../svm
	../../render
	../../util
	../../device
)

set(SRC
	background.cpp
	bsdf_ashikhmin_velvet.cpp
	bsdf_diffuse.cpp
	bsdf_oren_nayar.cpp
	bsdf_microfacet.cpp
	bsdf_reflection.cpp
	bsdf_refraction.cpp
	bsdf_transparent.cpp
	bsdf_ward.cpp
	bsdf_westin.cpp
	bssrdf.cpp
	debug.cpp
	emissive.cpp
	osl_closures.cpp
	osl_services.cpp
	osl_shader.cpp
	vol_subsurface.cpp
)

set(HEADER_SRC
	osl_closures.h
	osl_globals.h
	osl_services.h
	osl_shader.h
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${RTTI_DISABLE_FLAGS}")

include_directories(${INC})

add_library(cycles_kernel_osl ${SRC} ${HEADER_SRC})

add_subdirectory(nodes)

if(WITH_CYCLES_OSL)
	target_link_libraries(cycles_kernel_osl ${OSL_LIBRARIES})
endif()
