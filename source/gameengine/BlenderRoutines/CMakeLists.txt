
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
	../../../extern/bullet2/src
	../../../intern/container
	../../../intern/guardedalloc
	../../../intern/moto/include
	../../../intern/string
	${GLEW_INCLUDE_PATH}
)

set(INC_SYS
	${PTHREADS_INCLUDE_DIRS}
)

set(SRC
	BL_KetsjiEmbedStart.cpp
	BL_System.cpp
	KX_BlenderCanvas.cpp
	KX_BlenderGL.cpp
	KX_BlenderInputDevice.cpp
	KX_BlenderKeyboardDevice.cpp
	KX_BlenderMouseDevice.cpp
	KX_BlenderRenderTools.cpp
	KX_BlenderSystem.cpp

	BL_System.h
	KX_BlenderCanvas.h
	KX_BlenderGL.h
	KX_BlenderInputDevice.h
	KX_BlenderKeyboardDevice.h
	KX_BlenderMouseDevice.h
	KX_BlenderRenderTools.h
	KX_BlenderSystem.h
)

add_definitions(-DGLEW_STATIC)

if(WITH_AUDASPACE)
	list(APPEND INC
		../../../intern/audaspace/intern
	)
	add_definitions(-DWITH_AUDASPACE)
endif()

if(WITH_CODEC_FFMPEG)
	add_definitions(-DWITH_FFMPEG)
endif()

blender_add_lib(ge_blen_routines "${SRC}" "${INC}" "${INC_SYS}")
