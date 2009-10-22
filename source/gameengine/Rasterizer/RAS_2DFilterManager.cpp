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
#include "RAS_Rect.h"
#include "RAS_2DFilterManager.h"
#include <iostream>

#include "GL/glew.h"

#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "Value.h"

RAS_2DFilterManager::RAS_2DFilterManager():
texturewidth(-1), textureheight(-1),
canvaswidth(-1), canvasheight(-1),
numberoffilters(0), need_tex_update(true)
{
	isshadersupported = GLEW_ARB_shader_objects &&
		GLEW_ARB_fragment_shader && GLEW_ARB_multitexture;

	/* used to return before 2.49 but need to initialize values so dont */
	if(!isshadersupported)
		std::cout<<"shaders not supported!" << std::endl;

	int passindex;
	for(passindex =0; passindex<MAX_RENDER_PASS; passindex++)
	{
		m_filters[passindex] = 0;
		m_enabled[passindex] = 0;
		texflag[passindex] = 0;
		m_gameObjects[passindex] = NULL;
	}
	texname[0] = texname[1] = texname[2] = -1;
	errorprinted= false;
}

RAS_2DFilterManager::~RAS_2DFilterManager()
{
	FreeTextures();
}

void RAS_2DFilterManager::PrintShaderErrors(unsigned int shader, const char *task, const char *code)
{
	GLcharARB log[5000];
	GLsizei length = 0;
	const char *c, *pos, *end;
	int line = 1;

	if(errorprinted)
		return;
	
	errorprinted= true;

	glGetInfoLogARB(shader, sizeof(log), &length, log);
	end = code + strlen(code);

	printf("2D Filter GLSL Shader: %s error:\n", task);

	c = code;
	while ((c < end) && (pos = strchr(c, '\n'))) {
		printf("%2d  ", line);
		fwrite(c, (pos+1)-c, 1, stdout);
		c = pos+1;
		line++;
	}
	printf("%s", c);

	printf("%s\n", log);
}

unsigned int RAS_2DFilterManager::CreateShaderProgram(const char* shadersource)
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
		PrintShaderErrors(fShader, "compile", shadersource);
		return 0;
	}
		
	program = glCreateProgramObjectARB();
	glAttachObjectARB(program, fShader);

	glLinkProgramARB(program);
	glGetObjectParameterivARB(program, GL_LINK_STATUS, &success);
	if (!success)
	{
		/*Program Link Error*/
		PrintShaderErrors(fShader, "link", shadersource);
		return 0;
	}
	
	glValidateProgramARB(program);
	glGetObjectParameterivARB(program, GL_VALIDATE_STATUS, &success);
	if (!success)
	{
		/*Program Validation Error*/
		PrintShaderErrors(fShader, "validate", shadersource);
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

void RAS_2DFilterManager::AnalyseShader(int passindex, vector<STR_String>& propNames)
{
	texflag[passindex] = 0;
	if(glGetUniformLocationARB(m_filters[passindex], "bgl_DepthTexture") != -1)
	{
		if(GLEW_ARB_depth_texture)
			texflag[passindex] |= 0x1;
	}
	if(glGetUniformLocationARB(m_filters[passindex], "bgl_LuminanceTexture") != -1)
	{
		texflag[passindex] |= 0x2;
	}

	if(m_gameObjects[passindex])
	{
		int objProperties = propNames.size();
		int i;
		for(i=0; i<objProperties; i++)
			if(glGetUniformLocationARB(m_filters[passindex], propNames[i]) != -1)
				m_properties[passindex].push_back(propNames[i]);
	}
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
	if(texflag[passindex] & 0x2){
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

	int i, objProperties = m_properties[passindex].size();
	for(i=0; i<objProperties; i++)
	{
		uniformLoc = glGetUniformLocationARB(m_filters[passindex], m_properties[passindex][i]);
		if(uniformLoc != -1)
		{
			float value = ((CValue*)m_gameObjects[passindex])->GetPropertyNumber(m_properties[passindex][i], 0.0);
			glUniform1fARB(uniformLoc,value);
		}
	}
}

void RAS_2DFilterManager::EndShaderProgram()
{
	glUseProgramObjectARB(0);
}

void RAS_2DFilterManager::FreeTextures()
{
	if(texname[0]!=(unsigned int)-1)
		glDeleteTextures(1, (GLuint*)&texname[0]);
	if(texname[1]!=(unsigned int)-1)
		glDeleteTextures(1, (GLuint*)&texname[1]);
	if(texname[2]!=(unsigned int)-1)
		glDeleteTextures(1, (GLuint*)&texname[2]);
}

void RAS_2DFilterManager::SetupTextures(bool depth, bool luminance)
{
	FreeTextures();
	
	glGenTextures(1, (GLuint*)&texname[0]);
	glBindTexture(GL_TEXTURE_2D, texname[0]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texturewidth, textureheight, 0, GL_RGBA,
			GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	if(depth){
		glGenTextures(1, (GLuint*)&texname[1]);
		glBindTexture(GL_TEXTURE_2D, texname[1]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, texturewidth,textureheight, 
			0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE,NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE,
		                GL_NONE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	}

	if(luminance){
		glGenTextures(1, (GLuint*)&texname[2]);
		glBindTexture(GL_TEXTURE_2D, texname[2]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE16, texturewidth, textureheight,
			 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	}
}

void RAS_2DFilterManager::UpdateOffsetMatrix(RAS_ICanvas* canvas)
{
	RAS_Rect canvas_rect = canvas->GetWindowArea();
	canvaswidth = canvas->GetWidth();
	canvasheight = canvas->GetHeight();

	texturewidth = canvaswidth + canvas_rect.GetLeft();
	textureheight = canvasheight + canvas_rect.GetBottom();
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
}

void RAS_2DFilterManager::UpdateCanvasTextureCoord(unsigned int * viewport)
{
	/*
	This function update canvascoord[].
	These parameters are used to create texcoord[1]
	That way we can access the texcoord relative to the canvas:
	(0.0,0.0) bottom left, (1.0,1.0) top right, (0.5,0.5) center
	*/
	canvascoord[0] = (GLfloat) viewport[0] / viewport[2];
	canvascoord[0] *= -1;
	canvascoord[1] = (GLfloat) (texturewidth - viewport[0]) / viewport[2];
 
	canvascoord[2] = (GLfloat) viewport[1] / viewport[3];
	canvascoord[2] *= -1;
	canvascoord[3] = (GLfloat)(textureheight - viewport[1]) / viewport[3];
}

void RAS_2DFilterManager::RenderFilters(RAS_ICanvas* canvas)
{
	bool need_depth=false;
	bool need_luminance=false;
	int num_filters = 0;

	int passindex;

	if(!isshadersupported)
		return;

	for(passindex =0; passindex<MAX_RENDER_PASS; passindex++)
	{
		if(m_filters[passindex] && m_enabled[passindex]){
			num_filters ++;
			if(texflag[passindex] & 0x1)
				need_depth = true;
			if(texflag[passindex] & 0x2)
				need_luminance = true;
			if(need_depth && need_luminance)
				break;
		}
	}

	if(num_filters <= 0)
		return;

	GLuint	viewport[4]={0};
	glGetIntegerv(GL_VIEWPORT,(GLint *)viewport);

	if(canvaswidth != canvas->GetWidth() || canvasheight != canvas->GetHeight())
	{
		UpdateOffsetMatrix(canvas);
		UpdateCanvasTextureCoord((unsigned int*)viewport);
		need_tex_update = true;
	}
	
	if(need_tex_update)
	{
		SetupTextures(need_depth, need_luminance);
		need_tex_update = false;
	}

	if(need_depth){
		glActiveTextureARB(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, texname[1]);
		glCopyTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT, 0, 0, texturewidth,textureheight, 0);
	}
	
	if(need_luminance){
		glActiveTextureARB(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, texname[2]);
		glCopyTexImage2D(GL_TEXTURE_2D,0,GL_LUMINANCE16, 0, 0, texturewidth,textureheight, 0);
	}

	glViewport(0,0, texturewidth, textureheight);

	glDisable(GL_DEPTH_TEST);
	glPushMatrix();		//GL_MODELVIEW
	glLoadIdentity();	// GL_MODELVIEW
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	for(passindex =0; passindex<MAX_RENDER_PASS; passindex++)
	{
		if(m_filters[passindex] && m_enabled[passindex])
		{
			StartShaderProgram(passindex);

			glActiveTextureARB(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, texname[0]);
			glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, texturewidth, textureheight, 0);
			glClear(GL_COLOR_BUFFER_BIT);

			glBegin(GL_QUADS);
				glColor4f(1.f, 1.f, 1.f, 1.f);
				glTexCoord2f(1.0, 1.0);	glMultiTexCoord2fARB(GL_TEXTURE3_ARB, canvascoord[1], canvascoord[3]); glVertex2f(1,1);
				glTexCoord2f(0.0, 1.0);	glMultiTexCoord2fARB(GL_TEXTURE3_ARB, canvascoord[0], canvascoord[3]); glVertex2f(-1,1);
				glTexCoord2f(0.0, 0.0);	glMultiTexCoord2fARB(GL_TEXTURE3_ARB, canvascoord[0], canvascoord[2]); glVertex2f(-1,-1);
				glTexCoord2f(1.0, 0.0);	glMultiTexCoord2fARB(GL_TEXTURE3_ARB, canvascoord[1], canvascoord[2]); glVertex2f(1,-1);
			glEnd();
		}
	}

	glEnable(GL_DEPTH_TEST);
	glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
	EndShaderProgram();	
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}

void RAS_2DFilterManager::EnableFilter(vector<STR_String>& propNames, void* gameObj, RAS_2DFILTER_MODE mode, int pass, STR_String& text)
{
	if(!isshadersupported)
		return;
	if(pass<0 || pass>=MAX_RENDER_PASS)
		return;
	need_tex_update = true;
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
		m_gameObjects[pass] = NULL;
		m_properties[pass].clear();
		texflag[pass] = 0;
		return;
	}
	
	if(mode == RAS_2DFILTER_CUSTOMFILTER)
	{
		if(m_filters[pass])
			glDeleteObjectARB(m_filters[pass]);
		m_filters[pass] = CreateShaderProgram(text.Ptr());
		m_gameObjects[pass] = gameObj;
		AnalyseShader(pass, propNames);
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
