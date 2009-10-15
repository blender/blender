#include "GLBlendEquation.h"

void FRS_glBlendEquation(GLenum mode) {
	if( glBlendEquation ) {
		glBlendEquation(mode);
	} else if ( glBlendEquationEXT ) {
		glBlendEquationEXT(mode);
	}
}
