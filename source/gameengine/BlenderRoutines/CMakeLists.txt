
SET(INC
	.
	../../../source/kernel/gen_system
	../../../intern/string
	../../../intern/guardedalloc
	../../../intern/audaspace/intern
	../../../source/gameengine/Rasterizer/RAS_OpenGLRasterizer 
	../../../source/gameengine/Converter
	../../../source/blender/imbuf
	../../../intern/moto/include
	../../../source/gameengine/Ketsji 
	../../../source/blender/blenlib
	../../../source/blender/blenkernel 
	../../../source/blender/blenfont
	../../../source/blender/editors/include
	../../../source/blender/windowmanager
	../../../source/blender
	../../../source/blender/makesdna
	../../../source/blender/makesrna 
	../../../source/gameengine/Rasterizer 
	../../../source/gameengine/GameLogic
	../../../source/gameengine/Expressions 
	../../../source/gameengine/Network
	../../../source/gameengine/SceneGraph 
	../../../source/gameengine/Physics/common
	../../../source/gameengine/Physics/Bullet
	../../../source/gameengine/Network/LoopBackNetwork
	../../../source/blender/blenloader
	../../../source/blender/gpu
	../../../extern/bullet2/src
	../../../extern/glew/include
)

SET(SRC
	BL_KetsjiEmbedStart.cpp
	KX_BlenderCanvas.cpp
	KX_BlenderGL.cpp
	KX_BlenderInputDevice.cpp
	KX_BlenderKeyboardDevice.cpp
	KX_BlenderMouseDevice.cpp
	KX_BlenderRenderTools.cpp
	KX_BlenderSystem.cpp
)

ADD_DEFINITIONS(-DGLEW_STATIC)

IF(WITH_FFMPEG)
	ADD_DEFINITIONS(-DWITH_FFMPEG)
ENDIF(WITH_FFMPEG)

IF(WITH_PYTHON)
	LIST(APPEND INC ${PYTHON_INC})
	ADD_DEFINITIONS(-DWITH_PYTHON)
ENDIF(WITH_PYTHON)

BLENDERLIB(bf_blroutines "${SRC}" "${INC}")
