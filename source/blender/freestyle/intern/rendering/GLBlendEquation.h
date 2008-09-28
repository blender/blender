#ifndef  GLBLENDEQUATION_H
#define GLBLENDEQUATION_H

#include <GL/glew.h>
#ifdef WIN32
# include <windows.h>
#endif
#ifdef __MACH__
# include <OpenGL/gl.h>
#else
# include <GL/gl.h>
#endif

void FRS_glBlendEquation(GLenum mode);

#endif // GLBLENDEQUATION_H
