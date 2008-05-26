///////////////////////////////////////////////////////////////////
//               libQGLViewer configuration file                 //
//  Modify these settings according to your local configuration  //
///////////////////////////////////////////////////////////////////

#ifndef QGLVIEWER_CONFIG_H
#define QGLVIEWER_CONFIG_H





#include <map>
#include <ostream>

#include <math.h>
#include <iostream>

using namespace std;

#include "AppGLWidget_point.h"

# ifdef WIN32
#  include <windows.h>
# endif
# ifdef __MACH__
#  include <OpenGL/gl.h>
# else
#  include <GL/gl.h>
# endif

#ifdef __APPLE_CC__
	#include <GLUT/glut.h>
#else
	#include <GL/glut.h>
#endif

#ifndef Q_UNUSED
        #  define Q_UNUSED(x) (void)x;
#endif

template <typename T>
inline const T &qMin(const T &a, const T &b) { if (a < b) return a; return b; }
template <typename T>
inline const T &qMax(const T &a, const T &b) { if (a < b) return b; return a; }
template <typename T>
inline const T &qBound(const T &min, const T &val, const T &max)
{ return qMax(min, qMin(max, val)); }

#endif // QGLVIEWER_CONFIG_H
