
set(INC
	..
)

set(INC_SYS

)

set(SRC
	subd_dice.cpp
	subd_patch.cpp
	subd_split.cpp
	subd_patch_table.cpp
)

set(SRC_HEADERS
	subd_dice.h
	subd_patch.h
	subd_patch_table.h
	subd_split.h
)

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

cycles_add_library(cycles_subd ${SRC} ${SRC_HEADERS})
