# This file should be viewed as a -*- mode: Makefile -*-

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#			      W A R N I N G ! ! !                             #
#             a u t h o r i z e d    p e r s o n a l    o n l y               #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

include(../Config.pri)

TEMPLATE        = app
TARGET          = $${APPNAME}
debug: TARGET   = $${TARGET}_d
VERSION         = $${APPVERSION}
TARGET_VERSION_EXT = $${APPVERSION_MAJ}.$${APPVERSION_MID}



#
# CONFIG
#
#######################################

CONFIG          *= console qglviewer2 3ds$${LIB3DS_VERSION_MAJ}.$${LIB3DS_VERSION_MIN} python$${PYTHON_VERSION_MAJ}.$${PYTHON_VERSION_MIN} glut
win32: CONFIG	+= embed_manifest_exe 
QT += xml

exists (../libconfig.pri) {
  include (../libconfig.pri)
}

#
# BUILD DIRECTORIES
#
#######################################

BUILD_DIR       = ../../build

OBJECTS_DIR     = $${BUILD_DIR}/$${REL_OBJECTS_DIR}
DESTDIR         = $${BUILD_DIR}/$${REL_DESTDIR}
UI_DIR          = ui_dir

#!win32:PYTHON_DIR_REL        = build/$${REL_DESTDIR}/lib/python
#win32:PYTHON_DIR_REL        = build\\$${REL_DESTDIR}\\python
  
#
# LIBS
#
#######################################

!static {
  !win32 {
    lib_bundle {
      LIBS += -F$${BUILD_DIR}/$${REL_DESTDIR}/lib -framework $${LIB_GEOMETRY} -framework $${LIB_IMAGE} \
		-framework $${LIB_SCENE_GRAPH} -framework $${LIB_SYSTEM} \
		-framework $${LIB_WINGED_EDGE} -framework $${LIB_VIEW_MAP} \
		-framework $${LIB_RENDERING} -framework $${LIB_STROKE}
    } else {
      LIBS *= -L$${BUILD_DIR}/$${REL_DESTDIR}/lib \
                     -l$${LIB_SYSTEM} -l$${LIB_IMAGE} -l$${LIB_GEOMETRY} \
                     -l$${LIB_SCENE_GRAPH} -l$${LIB_WINGED_EDGE} -l$${LIB_VIEW_MAP} \
                     -l$${LIB_RENDERING} -l$${LIB_STROKE}
    }
  }

  win32:LIBS      *= $${DESTDIR}/$${LIB_SCENE_GRAPH}$${LIBVERSION}.lib \
                     $${DESTDIR}/$${LIB_SYSTEM}$${LIBVERSION}.lib \
                     $${DESTDIR}/$${LIB_WINGED_EDGE}$${LIBVERSION}.lib \
                     $${DESTDIR}/$${LIB_VIEW_MAP}$${LIBVERSION}.lib \
                     $${DESTDIR}/$${LIB_STROKE}$${LIBVERSION}.lib \
                     $${DESTDIR}/$${LIB_RENDERING}$${LIBVERSION}.lib \
                     $${DESTDIR}/$${LIB_GEOMETRY}$${LIBVERSION}.lib \
                     $${DESTDIR}/$${LIB_IMAGE}$${LIBVERSION}.lib
}

# irix-n32:LIBS           *= -l3ds -lglut -lQGLViewer -lpython$${PYTHON_VERSION_MAJ}.$${PYTHON_VERSION_MIN}
# mac:LIBS                *= -framework GLUT -lobjc -l3ds -lm -lQGLViewer -lpython$${PYTHON_VERSION_MAJ}.$${PYTHON_VERSION_MIN}

#
# INCLUDE PATH
#
#######################################

#INCLUDEPATH             *= ../geometry ../image ../scene_graph ../stroke ../system \
#                           ../view_map ../winged_edge ../rendering

#
# DEFINES
#
#######################################

DEFINES                 *= APPNAME=\\\"$${APPNAME}\\\" \
                           APPVERSION=\\\"$${APPVERSION}\\\" \
                           #ROOT_DIR=\\"$(FREESTYLE_DIR)\\" \
                           PYTHON_DIR_REL=\\\"$${PYTHON_DIR_REL}\\\"
                                  
#
# MOC DIRECTORY
#
#######################################

win32:MOCEXT            = win32
linux-g++:MOCEXT        = linux
cygwin-g++:MOCEXT       = cygwin
irix-n32:MOCEXT         = irix
mac:MOCEXT              = mac
MOC_DIR                 = moc_$$MOCEXT

#
# INSTALL
#
#######################################

EXE_DIR       = ../../
# install library
target.path   = $$EXE_DIR
# "make install" configuration options
INSTALLS     += target

#
# SOURCES, HEADERS & FORMS
#
#######################################


static {
  include(../system/src.pri)
  include(../image/src.pri)
  include(../geometry/src.pri)
  include(../scene_graph/src.pri)
  include(../winged_edge/src.pri)
  include(../view_map/src.pri)
  include(../stroke/src.pri)
  include(../rendering/src.pri)
}
#include(src.pri)
APP_DIR = ../app
DEPENDPATH += .
INCLUDEPATH += .

FORMS += appmainwindowbase4.ui \
         interactiveshaderwindow4.ui \
         optionswindow4.ui \
         progressdialog4.ui \
         stylewindow4.ui \
         densitycurveswindow4.ui
RESOURCES += $${APP_DIR}/freestyle.qrc
SOURCES *= $${APP_DIR}/AppAboutWindow.cpp \
           $${APP_DIR}/AppCanvas.cpp \
           $${APP_DIR}/AppConfig.cpp \
           $${APP_DIR}/AppGLWidget.cpp \
           $${APP_DIR}/AppInteractiveShaderWindow.cpp \
           $${APP_DIR}/AppMainWindow.cpp \
           $${APP_DIR}/AppOptionsWindow.cpp \
           $${APP_DIR}/AppProgressBar.cpp \
           $${APP_DIR}/AppStyleWindow.cpp \
           $${APP_DIR}/Controller.cpp \
           $${APP_DIR}/QGLBasicWidget.cpp \
           $${APP_DIR}/QStyleModuleSyntaxHighlighter.cpp \
           $${APP_DIR}/AppGL2DCurvesViewer.cpp \
           $${APP_DIR}/AppDensityCurvesWindow.cpp \
           $${APP_DIR}/ConfigIO.cpp \
           $${APP_DIR}/Main.cpp

HEADERS *= $${APP_DIR}/AppAboutWindow.h \
           $${APP_DIR}/AppCanvas.h \
           $${APP_DIR}/AppConfig.h \
           $${APP_DIR}/AppGLWidget.h \
           $${APP_DIR}/AppInteractiveShaderWindow.h \
           $${APP_DIR}/AppMainWindow.h \
           $${APP_DIR}/AppOptionsWindow.h \
           $${APP_DIR}/AppProgressBar.h \
           $${APP_DIR}/AppStyleWindow.h \
           $${APP_DIR}/QGLBasicWidget.h \
           $${APP_DIR}/QStyleModuleSyntaxHighlighter.h \
           $${APP_DIR}/AppGL2DCurvesViewer.h \
           $${APP_DIR}/AppDensityCurvesWindow.h \
	   $${APP_DIR}/ConfigIO.h \
           $${APP_DIR}/Controller.h

