
include_directories(. ../kernel ../kernel/svm ../render ../util ../device)

set(sources
	bvh.cpp
	bvh_build.cpp
	bvh_node.cpp
	bvh_sort.cpp)

set(headers
	bvh.h
	bvh_build.h
	bvh_node.h
	bvh_params.h
	bvh_sort.h)

add_library(cycles_bvh ${sources} ${headers})

