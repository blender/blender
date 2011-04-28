
INCLUDE_DIRECTORIES(. ../kernel ../kernel/svm ../render ../util ../device)

SET(sources
	bvh.cpp
	bvh_build.cpp
	bvh_node.cpp
	bvh_sort.cpp)

SET(headers
	bvh.h
	bvh_build.h
	bvh_node.h
	bvh_params.h
	bvh_sort.h)

ADD_LIBRARY(cycles_bvh ${sources} ${headers})

