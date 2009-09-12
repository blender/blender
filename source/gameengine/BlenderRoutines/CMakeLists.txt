
FILE(GLOB SRC *.cpp)

SET(INC
	.
	../../../source/kernel/gen_system
	../../../intern/string
	../../../intern/guardedalloc
	../../../source/gameengine/Rasterizer/RAS_OpenGLRasterizer 
	../../../source/gameengine/Converter
	../../../source/blender/imbuf
	../../../intern/ghost/include
	../../../intern/moto/include
	../../../source/gameengine/Ketsji 
	../../../source/blender/blenlib
	../../../source/blender/blenkernel 
	../../../source/blender/blenfont
	../../../source/blender/editors/include
	../../../source/blender/windowmanager
	../../../source/blender 
	../../../source/blender/include
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
	../../../source/blender/misc 
	../../../source/blender/blenloader
	../../../source/blender/gpu
	../../../extern/bullet2/src
	../../../extern/glew/include
	${PYTHON_INC}
)

IF(WITH_FFMPEG)
	ADD_DEFINITIONS(-DWITH_FFMPEG)
ENDIF(WITH_FFMPEG)

BLENDERLIB(bf_blroutines "${SRC}" "${INC}")
#env.BlenderLib ( 'bf_bloutines', sources, Split(incs), [], libtype=['game', 'game2', 'player'], priority=[0, 0, 55] , compileflags=cxxflags)
