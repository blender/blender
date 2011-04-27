
INCLUDE_DIRECTORIES(. ../ ../svm ../../render ../../util ../../device)

SET(sources
	background.cpp
	bsdf_ashikhmin_velvet.cpp
	bsdf_diffuse.cpp
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
	vol_subsurface.cpp)

SET(headers
	osl_closures.h
	osl_globals.h
	osl_services.h
	osl_shader.h)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${RTTI_DISABLE_FLAGS}")

ADD_LIBRARY(kernel_osl ${sources} ${headers})

ADD_SUBDIRECTORY(nodes)

