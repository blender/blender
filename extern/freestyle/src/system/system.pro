# This file should be viewed as a -*- mode: Makefile -*-

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#			      W A R N I N G ! ! !                             #
#             a u t h o r i z e d    p e r s o n a l    o n l y               #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

include(../Config.pri)

FOO_LIB_VERSION	= $${APPVERSION_MAJ}$${APPVERSION_MID}
TEMPLATE        = lib
TARGET          = $${LIB_SYSTEM}
VERSION         = $${APPVERSION}
TARGET_VERSION_EXT = $${APPVERSION_MAJ}.$${APPVERSION_MID}

#
# CONFIG
#
#######################################

CONFIG          *= dll python$${PYTHON_VERSION_MAJ}.$${PYTHON_VERSION_MIN}
QT += xml

exists (../libconfig.pri) {
  include (../libconfig.pri)
}
#
# INCLUDE PATH
#
#######################################


#
# DEFINES
#
#######################################

win32:DEFINES   *= MAKE_LIB_SYSTEM_DLL

#
# BUILD DIRECTORIES
#
#######################################

BUILD_DIR       = ../../build/

OBJECTS_DIR     = $${BUILD_DIR}/$${REL_OBJECTS_DIR}
!win32:DESTDIR  = $${BUILD_DIR}/$${REL_DESTDIR}/lib
win32:DESTDIR   = $${BUILD_DIR}/$${REL_DESTDIR}

#win32:QMAKE_POST_LINK = "$$QMAKE_MOVE $${DESTDIR}/$${TARGET}$${LIBVERSION}.lib $${DESTDIR}\$${TARGET}$${FOO_LIB_VERSION}.lib"

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

