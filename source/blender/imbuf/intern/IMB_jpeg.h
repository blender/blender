/*
 * IMB_jpeg.h
 *
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
/**
 * \file IMB_jpeg.h
 * \ingroup imbuf
 * \brief Function declarations for jpeg.c
 */

#ifndef IMB_JPEG_H
#define IMB_JPEG_H

struct ImBuf;
struct jpeg_compress_struct;

int imb_is_a_jpeg(unsigned char *mem);
int imb_savejpeg(struct ImBuf * ibuf, char * name, int flags);
struct ImBuf * imb_ibJpegImageFromFilename (const char * filename, int flags);
struct ImBuf * imb_ibJpegImageFromMemory (unsigned char * buffer, int size, int flags);

#endif

