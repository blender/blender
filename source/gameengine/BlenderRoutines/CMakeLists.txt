
set(INC
	.
	../Converter
	../Expressions
	../GameLogic
	../Ketsji
	../Network
	../Network/LoopBackNetwork
	../Physics/Bullet
	../Physics/common
	../Rasterizer
	../Rasterizer/RAS_OpenGLRasterizer
	../SceneGraph
	../../blender
	../../blender/blenfont
	../../blender/blenkernel
	../../blender/blenlib
	../../blender/blenloader
	../../blender/editors/include
	../../blender/gpu
	../../blender/imbuf
	../../blender/makesdna
	../../blender/makesrna
	../../blender/windowmanager
	../../../intern/container
	../../../intern/glew-mx
	../../../intern/guardedalloc
	../../../intern/string
)

set(INC_SYS
	../../../intern/moto/include
	${PTHREADS_INCLUDE_DIRS}
	${GLEW_INCLUDE_PATH}
	${BOOST_INCLUDE_DIR}
)

set(SRC
	BL_KetsjiEmbedStart.cpp
	BL_System.cpp
	KX_BlenderCanvas.cpp
	KX_BlenderInputDevice.cpp
	KX_BlenderKeyboardDevice.cpp
	KX_BlenderMouseDevice.cpp
	KX_BlenderSystem.cpp

	BL_System.h
	KX_BlenderCanvas.h
	KX_BlenderInputDevice.h
	KX_BlenderKeyboardDevice.h
	KX_BlenderMouseDevice.h
	KX_BlenderSystem.h
)

add_definitions(${GL_DEFINITIONS})

if(WITH_AUDASPACE)
	list(APPEND INC
		../../../intern/audaspace/intern
	)
	add_definitions(-DWITH_AUDASPACE)
endif()

if(WITH_CODEC_FFMPEG)
	add_definitions(-DWITH_FFMPEG)
endif()

if(WITH_BULLET)
	list(APPEND INC_SYS
		${BULLET_INCLUDE_DIRS}
	)
	add_definitions(-DWITH_BULLET)
endif()


blender_add_lib(ge_blen_routines "${SRC}" "${INC}" "${INC_SYS}")
