/**
 * $Id$
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

/**

 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 */


#include <stdio.h>



#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(WIN32) || defined(__APPLE__)
	#ifdef WIN32
        #if !defined(__CYGWIN32__)
        #pragma warning(disable:4244)
        #endif /* __CYGWIN32__ */
		#include <windows.h>
		#include <GL/gl.h>
	#else // WIN32
		// __APPLE__ is defined
		#include <AGL/gl.h>
	#endif // WIN32
#else // defined(WIN32) || defined(__APPLE__)
	#include <GL/gl.h>
#endif // defined(WIN32) || defined(__APPLE__)

#include "BMF_BitmapFont.h"


BMF_BitmapFont::BMF_BitmapFont(BMF_FontData* fontData)
: m_fontData(fontData)
{
}


BMF_BitmapFont::~BMF_BitmapFont(void)
{
}

void BMF_BitmapFont::DrawString(char* str)
{
	GLint alignment;
	unsigned char c;

	glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	
	while ( (c = (unsigned char) *str++) ) {
		BMF_CharData & cd = m_fontData->chars[c];
		
		if (cd.data_offset==-1) {
			GLubyte nullBitmap = 0;
		
			glBitmap(1, 1, 0, 0, cd.advance, 0, &nullBitmap);	
		} else {
			GLubyte *bitmap = &m_fontData->bitmap_data[cd.data_offset];
		
			glBitmap(cd.width, cd.height, cd.xorig, cd.yorig, cd.advance, 0, bitmap);
		}
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
}


int BMF_BitmapFont::GetStringWidth(char* str)
{
	unsigned char c;
	int length = 0;

	while ( (c = (unsigned char) *str++) ) {
		length += m_fontData->chars[c].advance;
	}
	
	return length;
}

void BMF_BitmapFont::GetFontBoundingBox(int & xMin, int & yMin, int & xMax, int & yMax)
{
	xMin = m_fontData->xmin;
	yMin = m_fontData->ymin;
	xMax = m_fontData->xmax;
	yMax = m_fontData->ymax;
}

int BMF_BitmapFont::GetFontHeight( void )
{
	return m_fontData->ymax - m_fontData->ymin;
}

void BMF_BitmapFont::GetStringBoundingBox(char* str, float*llx, float *lly, float *urx, float *ury)
{
	unsigned char c;
	int length = 0;
	int ascent = 0;
	int descent = 0;

	while ( (c = (unsigned char) *str++) ) {
		length += m_fontData->chars[c].advance;
		int d = m_fontData->chars[c].yorig;
		int a = m_fontData->chars[c].height - m_fontData->chars[c].yorig;
		if(a > ascent) ascent = a;
		if(d > descent) descent = d;
	}
	*llx = (float)0;
	*lly = (float)-descent;
	*urx = (float)length;
	*ury = (float)ascent;
}


int BMF_BitmapFont::GetTexture()
{
	int fWidth = m_fontData->xmax - m_fontData->xmin;
	int fHeight = m_fontData->ymax - m_fontData->ymin;
	
	if (fWidth>=16 || fHeight>=16) {
		return -1;
	}
	
	int cRows = 16, cCols = 16;
	int cWidth = 16, cHeight = 16;
	int iWidth = cCols*cWidth;
	int iHeight = cRows*cHeight;
	GLubyte *img = new GLubyte [iHeight*iWidth];
	GLuint texId;

	int baseLine = -(m_fontData->ymin);
	
	memset(img, 0, iHeight*iWidth);
	for (int i = 0; i<256; i++) {
		BMF_CharData & cd = m_fontData->chars[i];
		
		if (cd.data_offset != -1) {
			int cellX = i%16;
			int cellY = i/16;
			
			for (int y = 0; y<cd.height; y++) {
				GLubyte* imgRow = &img[(cellY*cHeight + y + baseLine - cd.yorig)*iWidth];
				GLubyte* chrRow = &m_fontData->bitmap_data[cd.data_offset + ((cd.width+7)/8)*y];

				for (int x = 0; x<cd.width; x++) {
					GLubyte* imgPxl = &imgRow[(cellX*cWidth + x - cd.xorig)];
					int byteIdx = x/8;
					int bitIdx = 7 - (x%8);
					
					if (chrRow[byteIdx]&(1<<bitIdx)) {
						imgPxl[0] = 255;
					}
				}
			}
		}
	}
	
	glGenTextures(1, &texId);
	
	glBindTexture(GL_TEXTURE_2D, texId);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA4, iWidth, iHeight, 0, GL_ALPHA, GL_UNSIGNED_BYTE, img);
	if (glGetError()) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE4_ALPHA4, iWidth, iHeight, 0, GL_ALPHA, GL_UNSIGNED_BYTE, img);
	}
	
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	
	delete [] img;
	
	return texId;
}

void BMF_BitmapFont::DrawStringTexture(char *str, float x, float y, float z)
{
	unsigned char c;
	float pos = 0;
	
	int baseLine = -(m_fontData->ymin);

	glBegin(GL_QUADS);
	while ( (c = (unsigned char) *str++) ) {
		BMF_CharData & cd = m_fontData->chars[c];
		
		if (cd.data_offset != -1) {
			float cellX = (c%16)/16.0;
			float cellY = (c/16)/16.0;
		
			glTexCoord2f(cellX + 1.0/16.0, cellY);
			glVertex3f(x + pos + 16.0, -baseLine + y + 0.0, z);

			glTexCoord2f(cellX + 1.0/16.0, cellY + 1.0/16.0);
			glVertex3f(x + pos + 16.0, -baseLine + y + 16.0, z);

			glTexCoord2f(cellX, cellY + 1.0/16.0);
			glVertex3f(x + pos + 0.0, -baseLine + y + 16.0, z);

			glTexCoord2f(cellX, cellY);
			glVertex3f(x + pos + 0.0, -baseLine + y + 0.0, z);
		}
		
		pos += cd.advance;
	}
	glEnd();
}

#define FTOCHAR(val) val<=0.0f?0: (val>=1.0f?255: (char)(255.0f*val))
void BMF_BitmapFont::DrawStringBuf(char *str, int posx, int posy, float *col, unsigned char *buf, float *fbuf, int w, int h)
{
	int x, y;
	
	if (buf==0 && fbuf==0)
		return;

	/*offset for font*/
	posx -= m_fontData->xmin;
	posy -= m_fontData->ymin;
	
	if (buf) {
		unsigned char colch[4];
		unsigned char *max, *pixel;
		unsigned char c;
		
		for (x=0; x<4; x++) {
			colch[x] = FTOCHAR(col[x]);
		}
		
		max = buf + (4 * (w * h));
		while ((c = (unsigned char) *str++)) {
			BMF_CharData & cd = m_fontData->chars[c];
			if (cd.data_offset != -1) { 
				for (y = 0; y < cd.height; y++) {
					unsigned char* chrRow = &m_fontData->bitmap_data[cd.data_offset + ((cd.width+7)/8)*y];
					for (x = cd.xorig; x < cd.width; x++) {
						pixel = buf + 4 * (((posy + y - cd.yorig) * w) + (posx + x));
						if ((pixel < max) && (pixel > buf)) {
							int byteIdx = x/8;
							int bitIdx = 7 - (x%8);
							
							if (chrRow[byteIdx]&(1<<bitIdx)) {
								pixel[0] = colch[0];
								pixel[1] = colch[1];
								pixel[2] = colch[2];
								pixel[4] = 1; /*colch[3];*/
								
							}
						}
					}
				}
			}
			posx += cd.advance;
		}
	}
	
	if (fbuf) {
		float *pixel, *max;
		unsigned char c;
		int xi, yi;
		
		max = fbuf + (4 * (w * h));
		
		while ((c = (unsigned char) *str++)) {
			BMF_CharData & cd = m_fontData->chars[c];
			if (cd.data_offset != -1) { 
				for (yi = 0; yi < cd.height; yi++) {
					unsigned char* chrRow = &m_fontData->bitmap_data[cd.data_offset + ((cd.width+7)/8)*yi];
					for (xi = cd.xorig; xi < cd.width; xi++) {
						pixel = fbuf + 4 * (((posy + yi - cd.yorig) * w) + (posx + xi));
						if ((pixel < max) && (pixel > fbuf)) {
							int byteIdx = xi/8;
							int bitIdx = 7 - (xi%8);
							
							if (chrRow[byteIdx]&(1<<bitIdx)) {
								pixel[0] = col[0];
								pixel[1] = col[1];
								pixel[2] = col[2];
								pixel[3] = 1; /*col[3];*/
							}
						}
					}
				}
			}
			posx += cd.advance;
		}
	}
}
