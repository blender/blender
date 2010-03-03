/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include <windows.h>
#include "png.h"
#else
#include <png.h>
#endif

#include "RAS_IPolygonMaterial.h"
#include "GPC_Canvas.h"

GPC_Canvas::TBannerId GPC_Canvas::s_bannerId = 0;


GPC_Canvas::GPC_Canvas(
	int width,
	int height
) : 
	m_width(width),
	m_height(height),
	m_bannersEnabled(false)
{
}


GPC_Canvas::~GPC_Canvas()
{
	DisposeAllBanners();
}


//  void GPC_Canvas::InitPostRenderingContext(void)
//  {
//  	glViewport(0, 0, m_width, m_height);
//  	glMatrixMode(GL_PROJECTION);
//  	glLoadIdentity();
	
//  	glOrtho(-2.0, 2.0, -2.0, 2.0, -20.0, 20.0);

//  	glMatrixMode(GL_MODELVIEW);
//  	glLoadIdentity();

//  	glEnable(GL_DEPTH_TEST);

//  	glDepthFunc(GL_LESS);

//  	glShadeModel(GL_SMOOTH);
//  }

void GPC_Canvas::Resize(int width, int height)
{
	m_width = width;
	m_height = height;
}

void GPC_Canvas::EndFrame()
{
	if (m_bannersEnabled)
		DrawAllBanners();	
}


void GPC_Canvas::ClearColor(float r, float g, float b, float a)
{
	::glClearColor(r,g,b,a);
}

void GPC_Canvas::SetViewPort(int x1, int y1, int x2, int y2)
{
		/* XXX, nasty, this needs to go somewhere else,
		 * but where... definitly need to clean up this
		 * whole canvas/rendertools mess.
		 */
	glEnable(GL_SCISSOR_TEST);

	glViewport(x1,y1,x2-x1 + 1,y2-y1 + 1);
	glScissor(x1,y1,x2-x1 + 1,y2-y1 + 1);
};


void GPC_Canvas::ClearBuffer(
	int type
){

	int ogltype = 0;
	if (type & RAS_ICanvas::COLOR_BUFFER )
		ogltype |= GL_COLOR_BUFFER_BIT;
	if (type & RAS_ICanvas::DEPTH_BUFFER )
		ogltype |= GL_DEPTH_BUFFER_BIT;

	::glClear(ogltype);
}


GPC_Canvas::TBannerId GPC_Canvas::AddBanner(
	unsigned int bannerWidth, unsigned int bannerHeight,
	unsigned int imageWidth, unsigned int imageHeight,
	unsigned char* imageData, 
	TBannerAlignment alignment, bool enabled)
{
	TBannerData banner;

	banner.alignment = alignment;
	banner.enabled = enabled;
	banner.displayWidth = bannerWidth;
	banner.displayHeight = bannerHeight;
	banner.imageWidth = imageWidth;
	banner.imageHeight = imageHeight;
	unsigned int bannerDataSize = imageWidth*imageHeight*4;
	banner.imageData = new unsigned char [bannerDataSize];
	::memcpy(banner.imageData, imageData, bannerDataSize);
	banner.textureName = 0;

	m_banners.insert(TBannerMap::value_type(++s_bannerId, banner));
	return s_bannerId;
}


void GPC_Canvas::DisposeBanner(TBannerId id)
{
	TBannerMap::iterator it = m_banners.find(id);
	if (it != m_banners.end()) {
		DisposeBanner(it->second);
		m_banners.erase(it);
	}
}

void GPC_Canvas::DisposeAllBanners()
{
	TBannerMap::iterator it = m_banners.begin();
	while (it != m_banners.end()) {
		DisposeBanner(it->second);
		it++;
	}
}

void GPC_Canvas::SetBannerEnabled(TBannerId id, bool enabled)
{
	TBannerMap::iterator it = m_banners.find(id);
	if (it != m_banners.end()) {
		it->second.enabled = enabled;
	}
}


void GPC_Canvas::SetBannerDisplayEnabled(bool enabled)
{
	m_bannersEnabled = enabled;
}


void GPC_Canvas::DisposeBanner(TBannerData& banner)
{
	if (banner.imageData) {
		delete [] banner.imageData;
		banner.imageData = 0;
	}
	if (banner.textureName) {
		::glDeleteTextures(1, (GLuint*)&banner.textureName);
	}
}

void GPC_Canvas::DrawAllBanners(void)
{
	if(!m_bannersEnabled || (m_banners.size() < 1))
		return;
	
	// Save the old rendering parameters.

	CanvasRenderState render_state;
	PushRenderState(render_state);

	// Set up everything for banner display.
	
	// Set up OpenGL matrices 
	SetOrthoProjection();
	// Activate OpenGL settings needed for display of the texture
	::glDisable(GL_LIGHTING);
	::glDisable(GL_DEPTH_TEST);
	::glDisable(GL_FOG);
	::glEnable(GL_TEXTURE_2D);
	::glEnable(GL_BLEND);
	::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	TBannerMap::iterator it = m_banners.begin();
	while (it != m_banners.end()) {
		if (it->second.enabled) {
			DrawBanner(it->second);
		}
		it++;
	}

	PopRenderState(render_state);
}


void GPC_Canvas::DrawBanner(TBannerData& banner)
{
	if(!banner.enabled)
		return;

	// Set up coordinates
	int coords[4][2];
	if (banner.alignment == alignTopLeft) {
		// Upper left
		coords[0][0] = 0;
		coords[0][1] = ((int)m_height)-banner.displayHeight;
		coords[1][0] = banner.displayWidth;
		coords[1][1] = ((int)m_height)-banner.displayHeight;
		coords[2][0] = banner.displayWidth;
		coords[2][1] = ((int)m_height);
		coords[3][0] = 0;
		coords[3][1] = ((int)m_height);
	}
	else {
		// Lower right
		coords[0][0] = (int)m_width - banner.displayWidth;
		coords[0][1] = 0;
		coords[1][0] = m_width;
		coords[1][1] = 0;
		coords[2][0] = m_width;
		coords[2][1] = banner.displayHeight;
		coords[3][0] = (int)m_width - banner.displayWidth;
		coords[3][1] = banner.displayHeight;
	}
	// Set up uvs
	int uvs[4][2] = {
		{ 0, 1},
		{ 1, 1},
		{ 1, 0},
		{ 0, 0}
	};

	if (!banner.textureName) {
		::glGenTextures(1, (GLuint*)&banner.textureName);
		::glBindTexture(GL_TEXTURE_2D, banner.textureName);
		::glTexImage2D(
			GL_TEXTURE_2D,			// target
			0,						// level
			4,						// components
			banner.imageWidth,		// width
			banner.displayHeight,	// height
			0,						// border
			GL_RGBA,				// format
			GL_UNSIGNED_BYTE,		// type
			banner.imageData);		// image data
		::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else {
		::glBindTexture(GL_TEXTURE_2D, banner.textureName);
	}

	// Draw the rectangle with the texture on it
	::glBegin(GL_QUADS);
	::glColor4f(1.f, 1.f, 1.f, 1.f);
	::glTexCoord2iv((GLint*)uvs[0]);
	::glVertex2iv((GLint*)coords[0]);
	::glTexCoord2iv((GLint*)uvs[1]);
	::glVertex2iv((GLint*)coords[1]);
	::glTexCoord2iv((GLint*)uvs[2]);
	::glVertex2iv((GLint*)coords[2]);
	::glTexCoord2iv((GLint*)uvs[3]);
	::glVertex2iv((GLint*)coords[3]);
	::glEnd();
}

	void
GPC_Canvas::
PushRenderState(
	CanvasRenderState & render_state
){
#if 0

	::glMatrixMode(GL_PROJECTION);
	::glPushMatrix();
	::glMatrixMode(GL_MODELVIEW);
	::glPushMatrix();
	::glMatrixMode(GL_TEXTURE);
	::glPushMatrix();
	// Save old OpenGL settings
	::glGetIntegerv(GL_LIGHTING, (GLint*)&(render_state.oldLighting));
	::glGetIntegerv(GL_DEPTH_TEST, (GLint*)&(render_state.oldDepthTest));
	::glGetIntegerv(GL_FOG, (GLint*)&(render_state.oldFog));
	::glGetIntegerv(GL_TEXTURE_2D, (GLint*)&(render_state.oldTexture2D));
	::glGetIntegerv(GL_BLEND, (GLint*)&(render_state.oldBlend));
	::glGetIntegerv(GL_BLEND_SRC, (GLint*)&(render_state.oldBlendSrc));
	::glGetIntegerv(GL_BLEND_DST, (GLint*)&(render_state.oldBlendDst));
	::glGetFloatv(GL_CURRENT_COLOR, render_state.oldColor);
	::glGetIntegerv(GL_DEPTH_WRITEMASK,(GLint*)&(render_state.oldWriteMask));
#else

	glPushAttrib(GL_ALL_ATTRIB_BITS);

#endif
}

	void
GPC_Canvas::
PopRenderState(
	const CanvasRenderState & render_state
){
#if 0
	// Restore OpenGL settings
	render_state.oldLighting ? ::glEnable(GL_LIGHTING) : glDisable(GL_LIGHTING);
	render_state.oldDepthTest ? ::glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
	render_state.oldFog ? ::glEnable(GL_FOG) : ::glDisable(GL_FOG);
	render_state.oldTexture2D ? ::glEnable(GL_TEXTURE_2D) : glDisable(GL_TEXTURE_2D);
	render_state.oldBlend ? glEnable(GL_BLEND) : ::glDisable(GL_BLEND);
	::glBlendFunc((GLenum)render_state.oldBlendSrc, (GLenum)render_state.oldBlendDst);
	render_state.oldWriteMask ? ::glEnable(GL_DEPTH_WRITEMASK) : glDisable(GL_DEPTH_WRITEMASK);

	::glColor4fv(render_state.oldColor);
	// Restore OpenGL matrices
	::glMatrixMode(GL_TEXTURE);
	::glPopMatrix();
	::glMatrixMode(GL_PROJECTION);
	::glPopMatrix();
	::glMatrixMode(GL_MODELVIEW);
	::glPopMatrix();

#else

	glPopAttrib();
#endif
}

	void
GPC_Canvas::
SetOrthoProjection(
){
	// Set up OpenGL matrices 
	::glViewport(0, 0, m_width, m_height);
	::glScissor(0, 0, m_width, m_height);
	::glMatrixMode(GL_PROJECTION);
	::glLoadIdentity();
	::glOrtho(0, m_width, 0, m_height, -1, 1);
	::glMatrixMode(GL_MODELVIEW);
	::glLoadIdentity();
	::glMatrixMode(GL_TEXTURE);
	::glLoadIdentity();
}

	void
GPC_Canvas::
MakeScreenShot(
	const char* filename
){
	png_structp png_ptr;
	png_infop info_ptr;
	unsigned char *pixels = 0;
	png_bytepp row_pointers = 0;
	int i, bytesperpixel = 3, color_type = PNG_COLOR_TYPE_RGB;
	FILE *fp = 0;

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) 
	{
		std::cout << "Cannot png_create_write_struct." << std::endl;
		return;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) 
	{
		png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
		std::cout << "Cannot png_create_info_struct." << std::endl;
		return;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		delete [] pixels;
		delete [] row_pointers;
		// printf("Aborting\n");
		if (fp) {
			fflush(fp);
			fclose(fp);
		}
		return;
	}

	// copy image data

	pixels = new unsigned char[GetWidth() * GetHeight() * bytesperpixel * sizeof(unsigned char)];
	if (!pixels) {
		std::cout << "Cannot allocate pixels array" << std::endl;
		return;
	}

	glReadPixels(0, 0, GetWidth(), GetHeight(), GL_RGB, GL_UNSIGNED_BYTE, pixels);

	fp = fopen(filename, "wb");
	if (!fp)
	{
		std::cout << "Couldn't open " << filename << " for writing." << std::endl;
		longjmp(png_jmpbuf(png_ptr), 1);
	}

	png_init_io(png_ptr, fp);

	/*
	png_set_filter(png_ptr, 0,
		PNG_FILTER_NONE  | PNG_FILTER_VALUE_NONE |
		PNG_FILTER_SUB   | PNG_FILTER_VALUE_SUB  |
		PNG_FILTER_UP    | PNG_FILTER_VALUE_UP   |
		PNG_FILTER_AVG   | PNG_FILTER_VALUE_AVG  |
		PNG_FILTER_PAETH | PNG_FILTER_VALUE_PAETH|
		PNG_ALL_FILTERS);

	png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
	*/

	// png image settings
	png_set_IHDR(png_ptr,
		 info_ptr,
		 GetWidth(),
		 GetHeight(),
		 8,
		 color_type,
		 PNG_INTERLACE_NONE,
		 PNG_COMPRESSION_TYPE_DEFAULT,
		 PNG_FILTER_TYPE_DEFAULT);

	// write the file header information
	png_write_info(png_ptr, info_ptr);

	// allocate memory for an array of row-pointers
	row_pointers = new png_bytep [(GetHeight() * sizeof(png_bytep))];
	if (!row_pointers) 
	{
		std::cout << "Cannot allocate row-pointers array" << std::endl;
		longjmp(png_jmpbuf(png_ptr), 1);
	}

	// set the individual row-pointers to point at the correct offsets
	for (i = 0; i < GetHeight(); i++) {
		row_pointers[GetHeight()-1-i] = (png_bytep)
			((unsigned char *)pixels + (i * GetWidth()) * bytesperpixel * sizeof(unsigned char));
	}

	// write out the entire image data in one call
	png_write_image(png_ptr, row_pointers);

	// write the additional chunks to the PNG file (not really needed)
	png_write_end(png_ptr, info_ptr);

	// clean up
	delete [] (pixels);
	delete [] (row_pointers);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	if (fp) 
	{
		fflush(fp);
		fclose(fp);
	}
}

