/*
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
 * Contributors: Amorilia (amorilia@users.sourceforge.net)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/imbuf/intern/dds/Image.cpp
 *  \ingroup imbdds
 */


/*
 * This file is based on a similar file from the NVIDIA texture tools
 * (http://nvidia-texture-tools.googlecode.com/)
 *
 * Original license from NVIDIA follows.
 */

// This code is in the public domain -- castanyo@yahoo.es

#include <Color.h>
#include <Image.h>

#include <stdio.h> // printf

Image::Image() : m_width(0), m_height(0), m_format(Format_RGB), m_data(NULL)
{
}

Image::~Image()
{
	free();
}

void Image::allocate(uint w, uint h)
{
	free();
	m_width = w;
	m_height = h;
	m_data = new Color32[w * h];
}

void Image::free()
{
	if (m_data) delete [] m_data;
	m_data = NULL;
}


uint Image::width() const
{
	return m_width;
}

uint Image::height() const
{
	return m_height;
}

const Color32 * Image::scanline(uint h) const
{
	if (h >= m_height) {
		printf("DDS: scanline beyond dimensions of image\n");
		return m_data;
	}
	return m_data + h * m_width;
}

Color32 *Image::scanline(uint h)
{
	if (h >= m_height) {
		printf("DDS: scanline beyond dimensions of image\n");
		return m_data;
	}
	return m_data + h * m_width;
}

const Color32 *Image::pixels() const
{
	return m_data;
}

Color32 *Image::pixels()
{
	return m_data;
}

const Color32 & Image::pixel(uint idx) const
{
	if (idx >= m_width * m_height) {
		printf("DDS: pixel beyond dimensions of image\n");
		return m_data[0];
	}
	return m_data[idx];
}

Color32 & Image::pixel(uint idx)
{
	if (idx >= m_width * m_height) {
		printf("DDS: pixel beyond dimensions of image\n");
		return m_data[0];
	}
	return m_data[idx];
}


Image::Format Image::format() const
{
	return m_format;
}

void Image::setFormat(Image::Format f)
{
	m_format = f;
}
