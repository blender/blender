
set(INC
	.
	../graph
	../kernel
	../kernel/svm
	../render
	../util
)

set(INC_SYS

)

set(SRC
	subd_dice.cpp
	subd_patch.cpp
	subd_split.cpp
)

set(SRC_HEADERS
	subd_dice.h
	subd_patch.h
	subd_split.h
)

if(WITH_CYCLES_OPENSUBDIV)
	add_definitions(-DWITH_OPENSUBDIV)
endif()

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

add_library(cycles_subd ${SRC} ${SRC_HEADERS})
