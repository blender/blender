#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32

#include "RAS_VAOpenGLRasterizer.h"
#include <windows.h>
#include "GL/gl.h"

typedef void (APIENTRY *GLLOCKARRAYSEXTPTR)(GLint first,GLsizei count);
typedef void (APIENTRY *GLUNLOCKARRAYSEXTPTR)(void);
void APIENTRY RAS_lockfunc(GLint first,GLsizei count) {};
void APIENTRY RAS_unlockfunc() {};
GLLOCKARRAYSEXTPTR glLockArraysEXT=RAS_lockfunc;
GLUNLOCKARRAYSEXTPTR glUnlockArraysEXT=RAS_unlockfunc;




#include "STR_String.h"
#include "RAS_TexVert.h"
#include "MT_CmMatrix4x4.h"
#include "RAS_IRenderTools.h" // rendering text

	
RAS_VAOpenGLRasterizer::RAS_VAOpenGLRasterizer(RAS_ICanvas* canvas)
:RAS_OpenGLRasterizer(canvas)
{
	int i = 0;
}



RAS_VAOpenGLRasterizer::~RAS_VAOpenGLRasterizer()
{
}



bool RAS_VAOpenGLRasterizer::Init()
{
	
	bool result = RAS_OpenGLRasterizer::Init();
	
	if (result)
	{
		// if possible, add extensions to other platforms too, if this
		// rasterizer becomes messy just derive one for each platform 
		// (ie. KX_Win32Rasterizer, KX_LinuxRasterizer etc.)
		
		glUnlockArraysEXT = reinterpret_cast<GLUNLOCKARRAYSEXTPTR>(wglGetProcAddress("glUnlockArraysEXT"));
		if (!glUnlockArraysEXT)
			result = false;
		
		glLockArraysEXT = reinterpret_cast<GLLOCKARRAYSEXTPTR>(wglGetProcAddress("glLockArraysEXT"));
		if (!glLockArraysEXT)
			result=false;
		
		glEnableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_NORMAL_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	return result;
}



void RAS_VAOpenGLRasterizer::SetDrawingMode(int drawingmode)
{
	m_drawingmode = drawingmode;

		switch (m_drawingmode)
	{
	case KX_BOUNDINGBOX:
		{
		}
	case KX_WIREFRAME:
		{
			glDisable (GL_CULL_FACE);
			break;
		}
	case KX_TEXTURED:
		{
		}
	case KX_SHADED:
		{
			glEnableClientState(GL_COLOR_ARRAY);
		}
	case KX_SOLID:
		{
			break;
		}
	default:
		{
		}
	}
}



void RAS_VAOpenGLRasterizer::Exit()
{
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisable(GL_COLOR_MATERIAL);

	RAS_OpenGLRasterizer::Exit();
}



void RAS_VAOpenGLRasterizer::IndexPrimitives( const vecVertexArray& vertexarrays,
							const vecIndexArrays & indexarrays,
							int mode,
							class RAS_IPolyMaterial* polymat,
							class RAS_IRenderTools* rendertools,
							bool useObjectColor,
							const MT_Vector4& rgbacolor)
{
	unsigned char* mypointer=NULL;
	static const GLsizei vtxstride = sizeof(RAS_TexVert);
	GLenum drawmode;
	switch (mode)
	{
	case 0:
		{
		drawmode = GL_TRIANGLES;
		break;
		}
	case 2:
		{
		drawmode = GL_QUADS;
		break;
		}
	case 1:	//lines
		{
		}
	default:
		{
		drawmode = GL_LINES;
		break;
		}
	}
	const RAS_TexVert* vertexarray;
	int numindices,vt;
	if (drawmode != GL_LINES)
	{
		if (useObjectColor)
		{
			glDisableClientState(GL_COLOR_ARRAY);
			glColor4d(rgbacolor[0], rgbacolor[1], rgbacolor[2], rgbacolor[3]);
		} else
		{
			glColor4d(0,0,0,1.0);
			glEnableClientState(GL_COLOR_ARRAY);
		}
	}
	else
	{
		glColor3d(0,0,0);
	}
	// use glDrawElements to draw each vertexarray
	for (vt=0;vt<vertexarrays.size();vt++)
	{
		vertexarray = &((*vertexarrays[vt]) [0]);
		const KX_IndexArray & indexarray = (*indexarrays[vt]);
		numindices = indexarray.size();
		int numverts = vertexarrays[vt]->size();

		if (!numindices)
			break;
		
		mypointer = (unsigned char*)(vertexarray);
		glVertexPointer(3,GL_FLOAT,vtxstride,mypointer);
		mypointer+= 3*sizeof(float);
		glTexCoordPointer(2,GL_FLOAT,vtxstride,mypointer);
		mypointer+= 2*sizeof(float);
		glColorPointer(4,GL_UNSIGNED_BYTE,vtxstride,mypointer);
		mypointer += sizeof(int);
		glNormalPointer(GL_SHORT,vtxstride,mypointer);
		glLockArraysEXT(0,numverts);
		// here the actual drawing takes places
		glDrawElements(drawmode,numindices,GL_UNSIGNED_INT,&(indexarray[0]));
		glUnlockArraysEXT();
	}
}


void RAS_VAOpenGLRasterizer::EnableTextures(bool enable)
{
	if (enable)
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	else
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

bool RAS_VAOpenGLRasterizer::Stereo()
{
/*
	if(m_stereomode == RAS_STEREO_NOSTEREO)
		return false;
	else
		return true;
*/
	return false;
}


#endif //WIN32
