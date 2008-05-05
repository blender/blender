# This file should be viewed as a -*- mode: Makefile -*-

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#			      W A R N I N G ! ! !                             #
#             a u t h o r i z e d    p e r s o n a l    o n l y               #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

include(../Config.pri)

TEMPLATE        = lib

TARGET          = $${LIB_RENDERING}
VERSION         = $${APPVERSION}
TARGET_VERSION_EXT = $${APPVERSION_MAJ}.$${APPVERSION_MID}

#
# CONFIG
#
#######################################

CONFIG          *= opengl glut dll 

exists (../libconfig.pri) {
  include (../libconfig.pri)
}

#
# DEFINES
#
#######################################

#DEFINES         *= ROOT_DIR=\"$(FREESTYLE_DIR)\"
win32:DEFINES   *= MAKE_LIB_RENDERING_DLL

#
# INCLUDE PATH
#
#######################################

#INCLUDEPATH     *= ../scene_graph ../winged_edge ../view_map ../geometry \
#                   ../stroke ../system ../image

#
# BUILD DIRECTORIES
#
#######################################

BUILD_DIR       = ../../build

OBJECTS_DIR     = $${BUILD_DIR}/$${REL_OBJECTS_DIR}
!win32:DESTDIR  = $${BUILD_DIR}/$${REL_DESTDIR}/lib
win32:DESTDIR   = $${BUILD_DIR}/$${REL_DESTDIR}

#
# LIBS
#
#######################################

win32:LIBS      *= $${DESTDIR}/$${LIB_GEOMETRY}$${LIBVERSION}.lib \
                   $${DESTDIR}/$${LIB_SCENE_GRAPH}$${LIBVERSION}.lib \
                   $${DESTDIR}/$${LIB_SYSTEM}$${LIBVERSION}.lib \
                   $${DESTDIR}/$${LIB_WINGED_EDGE}$${LIBVERSION}.lib \
                   $${DESTDIR}/$${LIB_VIEW_MAP}$${LIBVERSION}.lib \
                   $${DESTDIR}/$${LIB_STROKE}$${LIBVERSION}.lib

!win32 {
    lib_bundle {
      LIBS += -F$${DESTDIR} -framework $${LIB_GEOMETRY} \
		-framework $${LIB_IMAGE} -framework $${LIB_SCENE_GRAPH} \
		-framework $${LIB_SYSTEM} -framework $${LIB_WINGED_EDGE} \
		-framework $${LIB_VIEW_MAP} -framework $${LIB_STROKE}
    } else {
      LIBS *= -L$${DESTDIR} -l$${LIB_GEOMETRY} -l$${LIB_IMAGE} -l$${LIB_SCENE_GRAPH} \
                   -l$${LIB_SYSTEM} -l$${LIB_WINGED_EDGE} -l$${LIB_VIEW_MAP} -l$${LIB_STROKE}
    }
  }
#
# INSTALL
#
#######################################

LIB_DIR       = ../../lib
# install library
target.path   = $$LIB_DIR
# "make install" configuration options
INSTALLS     += target

#
# SOURCES & HEADERS
#
#######################################

!static {
  include(src.pri)
}


#
# DEFINES
# 
#######################################
!win32: DEFINES += GLX_GLXEXT_PROTOTYPES
