/**
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#define STRINGIFY(A)  #A

#include "RAS_OpenGLFilters/RAS_Blur2DFilter.h"
#include "RAS_OpenGLFilters/RAS_Sharpen2DFilter.h"
#include "RAS_OpenGLFilters/RAS_Dilation2DFilter.h"
#include "RAS_OpenGLFilters/RAS_Erosion2DFilter.h"
#include "RAS_OpenGLFilters/RAS_Laplacian2DFilter.h"
#include "RAS_OpenGLFilters/RAS_Sobel2DFilter.h"
#include "RAS_OpenGLFilters/RAS_Prewitt2DFilter.h"
#include "RAS_OpenGLFilters/RAS_GrayScale2DFilter.h"
#include "RAS_OpenGLFilters/RAS_Sepia2DFilter.h"
#include "RAS_OpenGLFilters/RAS_Invert2DFilter.h"

#include "STR_String.h"
#include "RAS_ICanvas.h"
#include "RAS_2DFilterManager.h"
#include <iostream>

#ifdef WIN32
// OpenGL gl.h needs 'windows.h' on windows platforms 
#include <windows.h>
#endif //WIN32
#ifdef __APPLE__
#define GL_GLEXT_LEGACY 1
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "RAS_OpenGLRasterizer/RAS_GLExtensionManager.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


RAS_2DFilterManager::RAS_2DFilterManager():
texturewidth(-1), textureheight(-1),
canvaswidth(-1), canvasheight(-1),
numberoffilters(0),texname(-1)
{
	isshadersupported = bgl::QueryVersion(2,0);
	if(!isshadersupported)
	{
		std::cout<<"shaders not supported!" << std::endl;
		return;
	}

	int passindex;
	for(passindex =0; passindex<MAX_RENDER_PASS; passindex++)
	{
		m_filters[passindex] = 0;
		m_enabled[passindex] = 0;
	}

}

RAS_2DFilterManager::~RAS_2DFilterManager()
{
}

unsigned int RAS_2DFilterManager::CreateShaderProgram(char* shadersource)
{
		GLuint program = 0;	
#if defined(GL_ARB_shader_objects) && defined(WITH_GLEXT)
		GLuint fShader = bgl::blCreateShaderObjectARB(GL_FRAGMENT_SHADER);
        GLint success;

		bgl::blShaderSourceARB(fShader, 1, (const char**)&shadersource, NULL);

		bgl::blCompileShaderARB(fShader);

		bgl::blGetObjectParameterivARB(fShader, GL_COMPILE_STATUS, &success);
		if(!success)
		{
			/*Shader Comile Error*/
			std::cout << "2dFilters - Shader compile error" << std::endl;
			return 0;
		}
		    
		program = bgl::blCreateProgramObjectARB();
		bgl::blAttachObjectARB(program, fShader);

		bgl::blLinkProgramARB(program);
		bgl::blGetObjectParameterivARB(program, GL_LINK_STATUS, &success);
		if (!success)
		{
			/*Program Link Error*/
			std::cout << "2dFilters - Shader program link error" << std::endl;
			return 0;
		}
   		
		bgl::blValidateProgramARB(program);
		bgl::blGetObjectParameterivARB(program, GL_VALIDATE_STATUS, &success);
        if (!success)
		{
			/*Program Validation Error*/
			std::cout << "2dFilters - Shader program validation error" << std::endl;
			return 0;
		}
#endif
		return program;
}

unsigned int RAS_2DFilterManager::CreateShaderProgram(int filtermode)
{
		switch(filtermode)
		{
			case RAS_2DFILTER_BLUR:
				return CreateShaderProgram(BlurFragmentShader);
			case RAS_2DFILTER_SHARPEN:
				return CreateShaderProgram(SharpenFragmentShader);
			case RAS_2DFILTER_DILATION:
				return CreateShaderProgram(DilationFragmentShader);
			case RAS_2DFILTER_EROSION:
				return CreateShaderProgram(ErosionFragmentShader);
			case RAS_2DFILTER_LAPLACIAN:
				return CreateShaderProgram(LaplacionFragmentShader);
			case RAS_2DFILTER_SOBEL:
				return CreateShaderProgram(SobelFragmentShader);
			case RAS_2DFILTER_PREWITT:
				return CreateShaderProgram(PrewittFragmentShader);
			case RAS_2DFILTER_GRAYSCALE:
				return CreateShaderProgram(GrayScaleFragmentShader);
			case RAS_2DFILTER_SEPIA:
				return CreateShaderProgram(SepiaFragmentShader);
			case RAS_2DFILTER_INVERT:
				return CreateShaderProgram(InvertFragmentShader);
		}
		return 0;
}

void RAS_2DFilterManager::StartShaderProgram(unsigned int shaderprogram)
{
#if defined(GL_ARB_shader_objects) && defined(WITH_GLEXT)
	GLint uniformLoc;
	bgl::blUseProgramObjectARB(shaderprogram);
	uniformLoc = bgl::blGetUniformLocationARB(shaderprogram, "bgl_RenderedTexture");
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texname);

    if (uniformLoc != -1)
    {
		bgl::blUniform1iARB(uniformLoc, 0);
    }
	uniformLoc = bgl::blGetUniformLocationARB(shaderprogram, "bgl_TextureCoordinateOffset");
    if (uniformLoc != -1)
    {
        bgl::blUniform2fvARB(uniformLoc, 9, textureoffsets);
    }
	uniformLoc = bgl::blGetUniformLocationARB(shaderprogram, "bgl_RenderedTextureWidth");
    if (uniformLoc != -1)
    {
		bgl::blUniform1fARB(uniformLoc,texturewidth);
    }
	uniformLoc = bgl::blGetUniformLocationARB(shaderprogram, "bgl_RenderedTextureHeight");
    if (uniformLoc != -1)
    {
		bgl::blUniform1fARB(uniformLoc,textureheight);
    }
#endif
}

void RAS_2DFilterManager::EndShaderProgram()
{
#if defined(GL_ARB_shader_objects) && defined(WITH_GLEXT)
	bgl::blUseProgramObjectARB(0);
#endif
}

void RAS_2DFilterManager::SetupTexture()
{
	if(texname!=-1)
	{
		glDeleteTextures(1,(const GLuint *)&texname);
	}
	glGenTextures(1, (GLuint *)&texname);
	glBindTexture(GL_TEXTURE_2D, texname);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texturewidth, textureheight, 0, GL_RGB,
		GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
}

void RAS_2DFilterManager::UpdateOffsetMatrix(int width, int height)
{
	canvaswidth = texturewidth = width;
	canvasheight = textureheight = height;

	GLint i,j;
	i = 0;
    while ((1 << i) <= texturewidth)
        i++;
    texturewidth = (1 << (i));

    // Now for height
    i = 0;
    while ((1 << i) <= textureheight)
        i++;
    textureheight = (1 << (i));

	GLfloat	xInc = 1.0f / (GLfloat)texturewidth;
	GLfloat yInc = 1.0f / (GLfloat)textureheight;
	
	for (i = 0; i < 3; i++)
	{
		for (j = 0; j < 3; j++)
		{
			textureoffsets[(((i*3)+j)*2)+0] = (-1.0f * xInc) + ((GLfloat)i * xInc);
			textureoffsets[(((i*3)+j)*2)+1] = (-1.0f * yInc) + ((GLfloat)j * yInc);
		}
	}

	SetupTexture();
}

void RAS_2DFilterManager::RenderFilters(RAS_ICanvas* canvas)
{
	if(!isshadersupported)
		return;

	if(canvaswidth != canvas->GetWidth() || canvasheight != canvas->GetHeight())
	{
		UpdateOffsetMatrix(canvas->GetWidth(), canvas->GetHeight());
	}
	GLuint	viewport[4]={0};

	int passindex;
	bool first = true;
	for(passindex =0; passindex<MAX_RENDER_PASS; passindex++)
	{
		if(m_filters[passindex] && m_enabled[passindex])
		{
			if(first)
			{
				glGetIntegerv(GL_VIEWPORT,(GLint *)viewport);
				glViewport(0, 0, texturewidth, textureheight);

				glDisable(GL_DEPTH_TEST);
				glMatrixMode(GL_PROJECTION);
				glLoadIdentity();
				glMatrixMode(GL_MODELVIEW);
				glLoadIdentity();
				first = false;
			}
			
			StartShaderProgram(m_filters[passindex]);

			glBindTexture(GL_TEXTURE_2D, texname);
			glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, texturewidth, textureheight, 0);

			glClear(GL_COLOR_BUFFER_BIT);

			glBegin(GL_QUADS);
				glColor4f(1.f, 1.f, 1.f, 1.f);
				glTexCoord2f(1.0, 1.0);	glVertex2f(1,1);
				glTexCoord2f(0.0, 1.0);	glVertex2f(-1,1);
				glTexCoord2f(0.0, 0.0);	glVertex2f(-1,-1);
				glTexCoord2f(1.0, 0.0);	glVertex2f(1,-1);
			glEnd();
		}
	}

	if(!first)
	{
		glEnable(GL_DEPTH_TEST);
		glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
		EndShaderProgram();	
	}
}

void RAS_2DFilterManager::EnableFilter(RAS_2DFILTER_MODE mode, int pass, STR_String& text)
{
	if(!isshadersupported)
		return;
#if defined(GL_ARB_shader_objects) && defined(WITH_GLEXT)
	if(pass<0 || pass>=MAX_RENDER_PASS)
		return;

	if(mode == RAS_2DFILTER_DISABLED)
	{
		m_enabled[pass] = 0;
		return;
	}

	if(mode == RAS_2DFILTER_ENABLED)
	{
		m_enabled[pass] = 1;
		return;
	}

	if(mode == RAS_2DFILTER_NOFILTER)
	{
		if(m_filters[pass])
			bgl::blDeleteObjectARB(m_filters[pass]);
		m_enabled[pass] = 0;
		m_filters[pass] = 0;
		return;
	}
	
	if(mode == RAS_2DFILTER_CUSTOMFILTER)
	{
		if(m_filters[pass])
			bgl::blDeleteObjectARB(m_filters[pass]);
		m_filters[pass] = CreateShaderProgram(text.Ptr());
		m_enabled[pass] = 1;
		return;
	}

	if(mode>=RAS_2DFILTER_MOTIONBLUR && mode<=RAS_2DFILTER_INVERT)
	{
		if(m_filters[pass])
			bgl::blDeleteObjectARB(m_filters[pass]);
		m_filters[pass] = CreateShaderProgram(mode);
		m_enabled[pass] = 1;
	}
#endif
}
