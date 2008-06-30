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

#include "GL/glew.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


RAS_2DFilterManager::RAS_2DFilterManager():
texturewidth(-1), textureheight(-1),
canvaswidth(-1), canvasheight(-1),
numberoffilters(0)
{
	isshadersupported = GLEW_ARB_shader_objects &&
		GLEW_ARB_fragment_shader && GLEW_ARB_multitexture;

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
		texflag[passindex] = 0;
	}
	texname[0] = texname[1] = texname[2] = -1;
}

RAS_2DFilterManager::~RAS_2DFilterManager()
{
}

unsigned int RAS_2DFilterManager::CreateShaderProgram(char* shadersource)
{
		GLuint program = 0;	
		GLuint fShader = glCreateShaderObjectARB(GL_FRAGMENT_SHADER);
        GLint success;

		glShaderSourceARB(fShader, 1, (const char**)&shadersource, NULL);

		glCompileShaderARB(fShader);

		glGetObjectParameterivARB(fShader, GL_COMPILE_STATUS, &success);
		if(!success)
		{
			/*Shader Comile Error*/
			std::cout << "2dFilters - Shader compile error" << std::endl;
			return 0;
		}
		    
		program = glCreateProgramObjectARB();
		glAttachObjectARB(program, fShader);

		glLinkProgramARB(program);
		glGetObjectParameterivARB(program, GL_LINK_STATUS, &success);
		if (!success)
		{
			/*Program Link Error*/
			std::cout << "2dFilters - Shader program link error" << std::endl;
			return 0;
		}
   		
		glValidateProgramARB(program);
		glGetObjectParameterivARB(program, GL_VALIDATE_STATUS, &success);
        if (!success)
		{
			/*Program Validation Error*/
			std::cout << "2dFilters - Shader program validation error" << std::endl;
			return 0;
		}

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

void RAS_2DFilterManager::StartShaderProgram(int passindex)
{
	GLint uniformLoc;
	glUseProgramObjectARB(m_filters[passindex]);
	uniformLoc = glGetUniformLocationARB(m_filters[passindex], "bgl_RenderedTexture");
	glActiveTextureARB(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texname[0]);

    if (uniformLoc != -1)
    {
		glUniform1iARB(uniformLoc, 0);
    }

    /* send depth texture to glsl program if it needs */
	if(texflag[passindex] & 0x1){
    	uniformLoc = glGetUniformLocationARB(m_filters[passindex], "bgl_DepthTexture");
    	glActiveTextureARB(GL_TEXTURE1);
    	glBindTexture(GL_TEXTURE_2D, texname[1]);

    	if (uniformLoc != -1)
    	{
    		glUniform1iARB(uniformLoc, 1);
    	}
    }

    /* send luminance texture to glsl program if it needs */
	if(texflag[passindex] & 0x1){
    	uniformLoc = glGetUniformLocationARB(m_filters[passindex], "bgl_LuminanceTexture");
    	glActiveTextureARB(GL_TEXTURE2);
    	glBindTexture(GL_TEXTURE_2D, texname[2]);

    	if (uniformLoc != -1)
    	{
    		glUniform1iARB(uniformLoc, 2);
    	}
	}
	
	uniformLoc = glGetUniformLocationARB(m_filters[passindex], "bgl_TextureCoordinateOffset");
    if (uniformLoc != -1)
    {
        glUniform2fvARB(uniformLoc, 9, textureoffsets);
    }
	uniformLoc = glGetUniformLocationARB(m_filters[passindex], "bgl_RenderedTextureWidth");
    if (uniformLoc != -1)
    {
		glUniform1fARB(uniformLoc,texturewidth);
    }
	uniformLoc = glGetUniformLocationARB(m_filters[passindex], "bgl_RenderedTextureHeight");
    if (uniformLoc != -1)
    {
		glUniform1fARB(uniformLoc,textureheight);
    }
}

void RAS_2DFilterManager::EndShaderProgram()
{
	glUseProgramObjectARB(0);
}

void RAS_2DFilterManager::SetupTexture()
{
	if(texname[0]!=-1 || texname[1]!=-1)
	{
		glDeleteTextures(2, (GLuint*)texname);
	}
	glGenTextures(3, (GLuint*)texname);

	glBindTexture(GL_TEXTURE_2D, texname[0]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texturewidth, textureheight, 0, GL_RGB,
			GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glBindTexture(GL_TEXTURE_2D, texname[1]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, texturewidth,textureheight, 0, GL_DEPTH_COMPONENT,
			GL_FLOAT,NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE,
		                GL_NONE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	
	glBindTexture(GL_TEXTURE_2D, texname[2]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE16, texturewidth, textureheight, 0, GL_LUMINANCE,
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
				/* this pass needs depth texture*/
				if(texflag[passindex] & 0x1){
					glActiveTextureARB(GL_TEXTURE1);
					glBindTexture(GL_TEXTURE_2D, texname[1]);
					glCopyTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT, 0,0, texturewidth,textureheight, 0);
				}
				
				/* this pass needs luminance texture*/
				if(texflag[passindex] & 0x2){
					glActiveTextureARB(GL_TEXTURE2);
					glBindTexture(GL_TEXTURE_2D, texname[2]);
					glCopyTexImage2D(GL_TEXTURE_2D,0,GL_LUMINANCE16, 0,0, texturewidth,textureheight, 0);
				}

				glGetIntegerv(GL_VIEWPORT,(GLint *)viewport);
				glViewport(0, 0, texturewidth, textureheight);

				glDisable(GL_DEPTH_TEST);
				glMatrixMode(GL_PROJECTION);
				glLoadIdentity();
				glMatrixMode(GL_MODELVIEW);
				glLoadIdentity();
				first = false;
			}
			
			StartShaderProgram(passindex);


			glActiveTextureARB(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, texname[0]);
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

void RAS_2DFilterManager::EnableFilter(RAS_2DFILTER_MODE mode, int pass, STR_String& text, short tflag)
{
	if(!isshadersupported)
		return;
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
			glDeleteObjectARB(m_filters[pass]);
		m_enabled[pass] = 0;
		m_filters[pass] = 0;
		texflag[pass] = 0;
		return;
	}
	
	if(mode == RAS_2DFILTER_CUSTOMFILTER)
	{
		texflag[pass] = tflag;
		if(m_filters[pass])
			glDeleteObjectARB(m_filters[pass]);
		m_filters[pass] = CreateShaderProgram(text.Ptr());
		m_enabled[pass] = 1;
		return;
	}

	if(mode>=RAS_2DFILTER_MOTIONBLUR && mode<=RAS_2DFILTER_INVERT)
	{
		if(m_filters[pass])
			glDeleteObjectARB(m_filters[pass]);
		m_filters[pass] = CreateShaderProgram(mode);
		m_enabled[pass] = 1;
	}
}
