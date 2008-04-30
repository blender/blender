# This file should be viewed as a -*- mode: Makefile -*-

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#			      W A R N I N G ! ! !                             #
#             a u t h o r i z e d    p e r s o n a l    o n l y               #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

include(../Config.pri)

TEMPLATE        = lib

TARGET          = $${LIB_STROKE}
VERSION         = $${APPVERSION}
TARGET_VERSION_EXT = $${APPVERSION_MAJ}.$${APPVERSION_MID}

#
# CONFIG
#
#######################################

CONFIG          *= dll

#
# DEFINES
#
#######################################

win32:DEFINES           *= MAKE_LIB_STROKE_DLL

#
# INCLUDE PATH
#
#######################################

#INCLUDEPATH     *= ../geometry ../image ../system ../view_map \
#                   ../winged_edge ../scene_graph

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

win32:LIBS              *= $${DESTDIR}/$${LIB_GEOMETRY}$${LIBVERSION}.lib \
                           $${DESTDIR}/$${LIB_IMAGE}$${LIBVERSION}.lib \
                           $${DESTDIR}/$${LIB_SCENE_GRAPH}$${LIBVERSION}.lib \
                           $${DESTDIR}/$${LIB_SYSTEM}$${LIBVERSION}.lib \
                           $${DESTDIR}/$${LIB_WINGED_EDGE}$${LIBVERSION}.lib \
                           $${DESTDIR}/$${LIB_VIEW_MAP}$${LIBVERSION}.lib

!win32 {
  lib_bundle {
    LIBS += -F$${DESTDIR} -framework $${LIB_GEOMETRY} -framework $${LIB_IMAGE} -framework $${LIB_SCENE_GRAPH} -framework $${LIB_SYSTEM} -framework $${LIB_WINGED_EDGE} -framework $${LIB_VIEW_MAP}
  } else {
    LIBS *= -L$${DESTDIR} -l$${LIB_GEOMETRY} -l$${LIB_IMAGE} -l$${LIB_SCENE_GRAPH} \
                   -l$${LIB_SYSTEM} -l$${LIB_WINGED_EDGE} -l$${LIB_VIEW_MAP}
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
