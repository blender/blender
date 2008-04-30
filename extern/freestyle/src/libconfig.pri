# This file should be viewed as a -*- mode: Makefile -*-

contains( CONFIG, 3ds1.20 ) {
  message ("Using 3ds 1.2.0 module")
  linux-g++:INCLUDEPATH  *= $(HOME)/INCLUDE/LINUX
  linux-g++:LIBS         *= -L$(HOME)/LIB/LINUX -l3ds
  cygwin-g++:INCLUDEPATH  *= /usr/lib/lib3ds-1.2.0
  cygwin-g++:LIBS         *= -L/usr/lib/lib3ds-1.2.0/lib3ds -l3ds
  mac:INCLUDEPATH *= /usr/local/include
  mac:LIBS         *= -l3ds
  mac:QMAKE_LIBDIR *= /usr/local/lib
  win32:    INCLUDEPATH  *= C:\include\lib3ds\1.2.0
  win32:    QMAKE_LIBDIR *= C:\lib\lib3ds\1.2.0
  win32:debug:    LIBS         *= lib3ds-120sd.lib
  win32:release:    LIBS         *= lib3ds-120s.lib
}

contains( CONFIG, 3ds1.30 ) {
  message ("Using 3ds 1.3.0 module")
  linux-g++:INCLUDEPATH  *= $(HOME)/INCLUDE/LINUX
  linux-g++:LIBS         *= -L$(HOME)/LIB/LINUX -l3ds
  cygwin-g++:INCLUDEPATH  *= /usr/lib/lib3ds-1.3.0
  cygwin-g++:LIBS         *= -L/usr/lib/lib3ds-1.3.0/lib3ds -l3ds
  mac:INCLUDEPATH *= /usr/local/include
  mac:LIBS         *= -l3ds
  mac:QMAKE_LIBDIR *= /usr/local/lib
  win32:    INCLUDEPATH  *= C:\include\lib3ds\1.3.0
  win32:    QMAKE_LIBDIR *= C:\lib\lib3ds\1.3.0
  win32:debug:    LIBS         *= lib3ds-1_3d.lib
  win32:release:    LIBS         *= lib3ds-1_3.lib
}

contains( CONFIG, qglviewer ) {
  message ("Using QGLViewer module")
  CONFIG *= qt thread opengl glut
  linux-g++:INCLUDEPATH *= $(HOME)/INCLUDE
  linux-g++:LIBS        *= -L$(HOME)/LIB/LINUX -lQGLViewer
  cygwin-g++:LIBS        *= -lQGLViewer
  win32:    INCLUDEPATH  *= $(HOME)\INCLUDE
  win32:    QMAKE_LIBDIR *= $(HOME)\LIB
  win32:    LIBS        *= QGLViewer.lib
}

contains( CONFIG, python2.3) {
  message ("Using python 2.3 module")
  linux-g++:INCLUDEPATH *= /usr/include/python2.3
  linux-g++:LIBS	*= -lpthread -lm -lutil	
  linux-g++:LIBS        *= -L/usr/local/lib/ -lpython2.3 -L$(HOME)/LIB/LINUX 
  win32:    INCLUDEPATH *= C:\python23\include
  win32:    QMAKE_LIBDIR *= C:\python23\libs
  win32:    LIBS        *= python23.lib
}

contains( CONFIG, python2.4) {
  message ("Using python 2.4 module")
  linux-g++:INCLUDEPATH *= /usr/include/python2.4
  linux-g++:LIBS	*= -lpthread -lm -lutil	
  linux-g++:LIBS        *= -L/usr/local/lib/ -lpython2.4 -L$(HOME)/LIB/LINUX
  cygwin-g++:INCLUDEPATH *= /usr/include/python2.4
  cygwin-g++:LIBS	*= -lpthread -lm -lutil	
  cygwin-g++:LIBS        *= -L/usr/lib/python2.4/config -lpython2.4 
  win32:    INCLUDEPATH *= C:\python24\include
  win32:    QMAKE_LIBDIR *= C:\python24\libs
  win32:    LIBS        *= python24.lib
}

contains( CONFIG, python2.5) {
  message ("Using python 2.5 module")
  linux-g++:INCLUDEPATH *= /usr/include/python2.5
  linux-g++:LIBS	*= -lpthread -lm -lutil	
  linux-g++:LIBS        *= -L/usr/local/lib/ -lpython2.5 -L$(HOME)/LIB/LINUX
  mac:	    INCLUDEPATH *= /usr/include/python2.5
  mac:      LIBS        *= -L/usr/lib/python2.5/config -lpython2.5
  cygwin-g++:INCLUDEPATH *= /usr/include/python2.5
  cygwin-g++:LIBS	*= -lpthread -lm -lutil	
  cygwin-g++:LIBS        *= -L/usr/lib/python2.5/config -lpython2.5 
  win32:    INCLUDEPATH *= C:\python25\include
  win32:    QMAKE_LIBDIR *= C:\python25\libs
  win32:    LIBS        *= python25.lib
}


contains( CONFIG, glut) {
  message ("Using glut module")
  linux-g++:LIBS *= -lglut -lXi
  cygwin-g++:LIBS *= -lglut -lXi
  mac: LIBS *= -framework Glut
  win32:INCLUDEPATH *= C:\include
  win32: QMAKE_LIBDIR *= C:\lib\glut
  win32: LIBS *= glut32.lib
}

contains( CONFIG, qglviewer2 ) {
  message ("Using QGLViewer module")
  CONFIG *= qt thread opengl glut
  linux-g++:INCLUDEPATH *= $(HOME)/INCLUDE
  linux-g++:LIBS        *= -L$(HOME)/LIB/LINUX -lQGLViewer
  mac: LIBS *= -lQGLViewer
  cygwin-g++:LIBS        *= -lQGLViewer2
  win32:    INCLUDEPATH  *= C:\include\QGLViewer\2.2.5
  win32{
    release{
      QMAKE_LIBDIR *= C:\lib\QGLViewer\release
    }
    debug{
      QMAKE_LIBDIR *= C:\lib\QGLViewer\debug
    }
  }
  win32:    LIBS        *= QGLViewer2.lib
}
