
set(INC
	.
	../kernel
	../kernel/svm
	../render
	../util
	../device
)

set(SRC
	bvh.cpp
	bvh_build.cpp
	bvh_node.cpp
	bvh_sort.cpp
)

set(SRC_HEADERS
	bvh.h
	bvh_build.h
	bvh_node.h
	bvh_params.h
	bvh_sort.h
)

include_directories(${INC})

add_library(cycles_bvh ${SRC} ${SRC_HEADERS})
