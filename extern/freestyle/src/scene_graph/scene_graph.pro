# This file should be viewed as a -*- mode: Makefile -*-

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#			      W A R N I N G ! ! !                             #
#             a u t h o r i z e d    p e r s o n a l    o n l y               #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

include(../Config.pri)

TEMPLATE        = lib

TARGET          = $${LIB_SCENE_GRAPH}
VERSION         = $${APPVERSION}
TARGET_VERSION_EXT = $${APPVERSION_MAJ}.$${APPVERSION_MID}

#
# CONFIG
#
#######################################

CONFIG          *= dll 3ds$${LIB3DS_VERSION_MAJ}.$${LIB3DS_VERSION_MIN}


exists (../libconfig.pri) {
  include (../libconfig.pri)
}
#
# DEFINES
#
#######################################

win32:DEFINES *= MAKE_LIB_SCENE_GRAPH_DLL

#
# INCLUDE PATH
#
#######################################

#INCLUDEPATH     *= ../geometry ../system

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
		   $${DESTDIR}/$${LIB_SYSTEM}$${LIBVERSION}.lib
!win32 {
  lib_bundle {
    LIBS += -F$${DESTDIR} -framework $${LIB_GEOMETRY} \
	    -framework $${LIB_SYSTEM}
  } else {
    LIBS *= -L$${DESTDIR}/ -l$${LIB_GEOMETRY} -l$${LIB_SYSTEM}
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
