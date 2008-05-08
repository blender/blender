# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#			      W A R N I N G ! ! !                             #
#             a u t h o r i z e d    p e r s o n a l    o n l y               #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

RENDERING_DIR = ../rendering

SOURCES *= $${RENDERING_DIR}/GLBBoxRenderer.cpp \
           $${RENDERING_DIR}/GLDebugRenderer.cpp \
           $${RENDERING_DIR}/GLFreeMemoryVisitor.cpp \
           $${RENDERING_DIR}/GLMonoColorRenderer.cpp \
           $${RENDERING_DIR}/GLRenderer.cpp \
           $${RENDERING_DIR}/GLSelectRenderer.cpp \
           $${RENDERING_DIR}/GLStrokeRenderer.cpp \
           $${RENDERING_DIR}/GLUtils.cpp 
				     
win32:SOURCES *= $${RENDERING_DIR}/extgl.cpp

#!win32:SOURCES *= $${RENDERING_DIR}/pbuffer.cpp 
				     
HEADERS *= $${RENDERING_DIR}/GLBBoxRenderer.h \
           $${RENDERING_DIR}/GLDebugRenderer.h \
           $${RENDERING_DIR}/GLFreeMemoryVisitor.h \
           $${RENDERING_DIR}/GLMonoColorRenderer.h \
           $${RENDERING_DIR}/GLRenderer.h \
           $${RENDERING_DIR}/GLSelectRenderer.h \
           $${RENDERING_DIR}/GLStrokeRenderer.h \
           $${RENDERING_DIR}/GLUtils.h 
				     
win32:HEADERS *= $${RENDERING_DIR}/extgl.h

#!win32:HEADERS *= $${RENDERING_DIR}/pbuffer.h

