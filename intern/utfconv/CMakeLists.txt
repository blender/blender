set(INC
	.
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

set(SRC
	utfconv.c

	utfconv.h
	#utf_func.h
)


if(WIN32)
	list(APPEND SRC
		utf_winfunc.c
		utf_winfunc.h
	)
endif()

blender_add_lib(bf_intern_utfconv "${SRC}" "${INC}" "${INC_SYS}")
