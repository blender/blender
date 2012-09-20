
set(INC
	.
	../util
	../kernel
	../kernel/svm
	../render
)
set(INC_SYS
)

set(SRC
	subd_build.cpp
	subd_dice.cpp
	subd_mesh.cpp
	subd_patch.cpp
	subd_ring.cpp
	subd_split.cpp
	subd_stencil.cpp
)

set(SRC_HEADERS
	subd_build.h
	subd_dice.h
	subd_edge.h
	subd_face.h
	subd_mesh.h
	subd_patch.h
	subd_ring.h
	subd_split.h
	subd_stencil.h
	subd_vert.h
)

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

add_library(cycles_subd ${SRC} ${SRC_HEADERS})
