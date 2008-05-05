# This file should be viewed as a -*- mode: Makefile -*-

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#                 A p p l i c a t i o n   &   L i b r a r i e s               #
#                    b u i l d    c o n f i g u r a t i o n                   #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

#
# APPLICATION
#
#######################################

APPNAME         = Freestyle
APPVERSION_MAJ  = 2
APPVERSION_MID  = 2
APPVERSION_MIN  = 0
APPVERSION      = $${APPVERSION_MAJ}.$${APPVERSION_MID}.$${APPVERSION_MIN}

PYTHON_VERSION_MAJ = 2
PYTHON_VERSION_MIN = 5

LIB3DS_VERSION_MAJ = 1
LIB3DS_VERSION_MIN = 30

#
# CONFIG
#
#######################################

CONFIG		-= debug release ReleaseBuild Release build_pass precompile_header debug_and_release debug_and_release_target
CONFIG            *= qt shared stl exceptions rtti thread
CONFIG                  *= release # debug or release
CONFIG                  *= warn_off # warn_off or warn_on
mac:CONFIG 		+= x86
#mac:CONFIG		+= ppc
#mac:CONFIG		*= lib_bundle

#mac:QMAKE_MAC_SDK = /Developer/SDKs/MacOSX10.4u.sdk
#message($$CONFIG)
#CONFIG                  *= static
#CONFIG                  *= profiling



QT += opengl

#
# LIBRARIES
#
#######################################

debug{
	LIB_GEOMETRY     = $${APPNAME}Geometry_d
	LIB_IMAGE        = $${APPNAME}Image_d
	LIB_RENDERING    = $${APPNAME}Rendering_d
	LIB_SCENE_GRAPH  = $${APPNAME}SceneGraph_d
	LIB_SYSTEM       = $${APPNAME}System_d
	LIB_VIEW_MAP     = $${APPNAME}ViewMap_d
	LIB_STROKE       = $${APPNAME}Stroke_d
	LIB_WINGED_EDGE  = $${APPNAME}WingedEdge_d
}else{
	LIB_GEOMETRY     = $${APPNAME}Geometry
	LIB_IMAGE        = $${APPNAME}Image
	LIB_RENDERING    = $${APPNAME}Rendering
	LIB_SCENE_GRAPH  = $${APPNAME}SceneGraph
	LIB_SYSTEM       = $${APPNAME}System
	LIB_VIEW_MAP     = $${APPNAME}ViewMap
	LIB_STROKE       = $${APPNAME}Stroke
	LIB_WINGED_EDGE  = $${APPNAME}WingedEdge
}

LIBVERSION	= $${APPVERSION_MAJ}.$${APPVERSION_MID}


#
# FLAGS
#
#######################################

win32:QMAKE_CXXFLAGS     *= /GR /GX
win32:QMAKE_CFLAGS       *= /GR /GX
irix-n32:QMAKE_CFLAGS    *= -LANG:std
irix-n32:QMAKE_CXXFLAGS  *= -LANG:std
linux-g++:QMAKE_CFLAGS   *= -Wno-deprecated
linux-g++:QMAKE_CXXFLAGS *= -Wno-deprecated
cygwin-g++:QMAKE_CFLAGS   *= -Wno-deprecated
cygwin-g++:QMAKE_CXXFLAGS *= -Wno-deprecated -mno-win32
mac:QMAKE_CFLAGS         *= -Wno-deprecated
mac:QMAKE_CXXFLAGS       *= -Wno-deprecated

linux-g++:QMAKE_CFLAGS_RELEASE   = -O3 -funroll-loops -fomit-frame-pointer -ffast-math -march=i686
linux-g++:QMAKE_CXXFLAGS_RELEASE = -O3 -funroll-loops -fomit-frame-pointer -ffast-math -march=i686
cygwin-g++:QMAKE_CFLAGS_RELEASE   = -O3 -funroll-loops -fomit-frame-pointer -ffast-math -march=i686
cygwin-g++:QMAKE_CXXFLAGS_RELEASE = -O3 -funroll-loops -fomit-frame-pointer -ffast-math -march=i686
mac:QMAKE_CFLAGS_RELEASE         = -O3 -funroll-loops -fomit-frame-pointer -ffast-math -march=i686
mac:QMAKE_CXXFLAGS_RELEASE       = -O3 -funroll-loops -fomit-frame-pointer -ffast-math -march=i686

profiling {
  linux-g++:QMAKE_CFLAGS_DEBUG   = -pg
  linux-g++:QMAKE_CXXFLAGS_DEBUG = -pg
  linux-g++:QMAKE_LFLAGS_DEBUG   = -pg
  cygwin-g++:QMAKE_CFLAGS_DEBUG   = -pg
  cygwin-g++:QMAKE_CXXFLAGS_DEBUG = -pg
  cygwin-g++:QMAKE_LFLAGS_DEBUG   = -pg
  mac:QMAKE_CFLAGS_DEBUG         = -pg
  mac:QMAKE_CXXFLAGS_DEBUG       = -pg
  mac:QMAKE_LFLAGS_DEBUG         = -pg
}

#
# DEFINES
#
#######################################

win32:DEFINES           *= WIN32 QT_DLL QT_THREAD_SUPPORT
linux-g++:DEFINES       *= LINUX
cygwin-g++:DEFINES      *= CYGWIN
irix-n32:DEFINES        *= IRIX
mac:DEFINES             *= MACOSX

#
# BUILD DIRECTORIES (RELATIVE)
#
#######################################

release {
  win32 {
    REL_OBJECTS_DIR   = \\win32\\release\\obj
    REL_DESTDIR       = \\win32\\release
  }
  linux-g++ {
    REL_OBJECTS_DIR   = linux-g++/release/obj
    REL_DESTDIR       = linux-g++/release
  }
  cygwin-g++ {
    REL_OBJECTS_DIR   = cygwin-g++/release/obj
    REL_DESTDIR       = cygwin-g++/release
  }
  irix-n32 {
    REL_OBJECTS_DIR   = irix-n32/release/obj
    REL_DESTDIR       = irix-n32/release
  }
  mac {
    REL_OBJECTS_DIR   = macosx/release/obj
    REL_DESTDIR       = macosx/release
  }
}
debug {
  win32 {
    REL_OBJECTS_DIR   = \\win32\\debug\\obj
    REL_DESTDIR       = \\win32\\debug
  }
  linux-g++ {
    REL_OBJECTS_DIR   = linux-g++/debug/obj
    REL_DESTDIR       = linux-g++/debug
  }
  cygwin-g++ {
    REL_OBJECTS_DIR   = cygwin-g++/debug/obj
    REL_DESTDIR       = cygwin-g++/debug
  }
  irix-n32 {
    REL_OBJECTS_DIR   = irix-n32/debug/obj
    REL_DESTDIR       = irix-n32/debug
  }
  mac {
    REL_OBJECTS_DIR   = macosx/debug/obj
    REL_DESTDIR       = macosx/debug
  }
}

#
# INSTALL
#
#######################################

#QMAKE_COPY_FILE       = $${QMAKE_COPY} -P
