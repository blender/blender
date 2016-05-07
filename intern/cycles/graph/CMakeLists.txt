
set(INC
	.
	../util
)

set(SRC
	node.cpp
	node_type.cpp
)

set(SRC_HEADERS
	node.h
	node_enum.h
	node_type.h
)

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

add_library(cycles_graph ${SRC} ${SRC_HEADERS})

