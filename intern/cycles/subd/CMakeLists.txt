
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
	subd_dice.cpp
	subd_mesh.cpp
	subd_patch.cpp
	subd_split.cpp
)

set(SRC_HEADERS
	subd_dice.h
	subd_mesh.h
	subd_patch.h
	subd_split.h
)

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

add_library(cycles_subd ${SRC} ${SRC_HEADERS})
