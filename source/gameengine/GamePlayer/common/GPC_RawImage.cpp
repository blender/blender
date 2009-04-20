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
   
#include <iostream>
#include <string.h>

#include "GPC_RawImage.h"
#include "GPC_RawLogoArrays.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


GPC_RawImage::GPC_RawImage()
		: m_data(0), m_dataSize(0), m_width(0), m_height(0)
{
}


bool GPC_RawImage::Load(
		const char *srcName,
		int destWidth, int destHeight,
		TImageAlignment alignment, int offsetX, int offsetY)
{
	int srcWidth, srcHeight;
	bool success = true;
	if(strcmp(srcName, "BlenderLogo") == 0)
		GetRawBlenderLogo(&m_data, &srcWidth, &srcHeight);
	else
		if(strcmp(srcName, "Blender3DLogo") == 0)
			GetRawBlender3DLogo(&m_data, &srcWidth, &srcHeight);
#if 0
	else
		if(strcmp(srcName, "NaNLogo") == 0)
			GetRawNaNLogo(&m_data, &srcWidth, &srcHeight);
#endif
		else  // unknown image
			success = false;

	if(success)
	{
		unsigned char *tempData = m_data;

		int numBytes = destWidth * destHeight * 4;
		m_data = new unsigned char[numBytes];  // re-use m_data ('unsigned char' was 'char')
		if(m_data)
		{
			::memset(m_data, 0x00000000, numBytes);
			m_width = destWidth;
			m_height = destHeight;

			int srcBytesWidth = srcWidth * 4;
			int dstBytesWidth = m_width * 4;
			int numRows = (srcHeight + offsetY) < m_height ? srcHeight : m_height - offsetY;
			numBytes = (srcWidth + offsetX) < m_width ? srcBytesWidth : (m_width - offsetX) * 4;

			if((offsetX < m_width) && (offsetY < m_height))
			{
				unsigned char* src = (unsigned char*)tempData;
				unsigned char* dst = (unsigned char*)m_data;
				if(alignment == alignTopLeft)
				{
					// Put original in upper left corner

					// Add vertical offset
					dst += offsetY * dstBytesWidth;	
					// Add horizontal offset
					dst += offsetX * 4;
					for (int row = 0; row < numRows; row++)
					{
						::memcpy(dst, src, numBytes);
						src += srcBytesWidth;
						dst += dstBytesWidth;
					}
				}
				else
				{
					// Put original in lower right corner

					// Add vertical offset
					dst += (m_height - (srcHeight + offsetY)) * dstBytesWidth;
					// Add horizontal offset
					if (m_width > (srcWidth + offsetX)) {
						dst += (m_width - (srcWidth + offsetX)) * 4;
					}
					else {
						src += (srcWidth + offsetX - m_width) * 4;
					}
					for (int row = 0; row < numRows; row++) {
						::memcpy(dst, src, numBytes);
						src += srcBytesWidth;
						dst += dstBytesWidth;
					}
				}
			}
// doesn't compile under Linux			delete [] tempData;
			delete tempData;
		}
		else {
			// Allocation failed, restore old data
			m_data = tempData;
			success = false;
		}
	}
	
	return success;
}

