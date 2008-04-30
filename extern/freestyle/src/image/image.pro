# This file should be viewed as a -*- mode: Makefile -*-

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#			      W A R N I N G ! ! !                             #
#             a u t h o r i z e d    p e r s o n a l    o n l y               #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

include(../Config.pri)

TEMPLATE        = lib

TARGET          = $${LIB_IMAGE}
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

win32:DEFINES   *= MAKE_LIB_IMAGE_DLL

#
# INCLUDE PATH
#
#######################################

#INCLUDEPATH     *= ../system

#
# BUILD DIRECTORIES
#
#######################################

BUILD_DIR       = ../../build

OBJECTS_DIR     = $${BUILD_DIR}/$${REL_OBJECTS_DIR}
!win32:DESTDIR  = $${BUILD_DIR}/$${REL_DESTDIR}/lib
win32:DESTDIR   = $${BUILD_DIR}/$${REL_DESTDIR}

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
