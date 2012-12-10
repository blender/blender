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

/** \file blender/imbuf/intern/dds/Stream.h
 *  \ingroup imbdds
 */


/* simple memory stream functions with buffer overflow check */

#ifndef __STREAM_H__
#define __STREAM_H__

struct Stream
{
	unsigned char *mem; // location in memory
	unsigned int size;  // size
	unsigned int pos;   // current position
	Stream(unsigned char *m, unsigned int s) : mem(m), size(s), pos(0) {}
	unsigned int seek(unsigned int p);
};

unsigned int mem_read(Stream & mem, unsigned long long & i);
unsigned int mem_read(Stream & mem, unsigned int & i);
unsigned int mem_read(Stream & mem, unsigned short & i);
unsigned int mem_read(Stream & mem, unsigned char & i);
unsigned int mem_read(Stream & mem, unsigned char *i, unsigned int cnt);

#endif  /* __STREAM_H__ */
