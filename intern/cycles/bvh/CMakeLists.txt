
set(INC
	..
)

set(INC_SYS
)

set(SRC
	bvh.cpp
	bvh2.cpp
	bvh4.cpp
	bvh_binning.cpp
	bvh_build.cpp
	bvh_node.cpp
	bvh_sort.cpp
	bvh_split.cpp
	bvh_unaligned.cpp
)

set(SRC_HEADERS
	bvh.h
	bvh2.h
	bvh4.h
	bvh_binning.h
	bvh_build.h
	bvh_node.h
	bvh_params.h
	bvh_sort.h
	bvh_split.h
	bvh_unaligned.h
)

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

add_library(cycles_bvh ${SRC} ${SRC_HEADERS})
