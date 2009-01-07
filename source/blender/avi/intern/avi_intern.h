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
#ifndef AVI_INTERN_H
#define AVI_INTERN_H

#include <stdio.h> /* for FILE */

#include "MEM_guardedalloc.h"

unsigned int GET_FCC (FILE *fp);
unsigned int GET_TCC (FILE *fp);

#define PUT_FCC(ch4, fp) putc(ch4[0],fp); putc(ch4[1],fp); putc(ch4[2],fp); putc(ch4[3],fp)
#define PUT_FCCN(num, fp) putc((num>>0)&0377,fp); putc((num>>8)&0377,fp); putc((num>>16)&0377,fp); putc((num>>24)&0377,fp)
#define PUT_TCC(ch2, fp) putc(ch2[0],fp); putc(ch2[1],fp)

void *avi_format_convert (AviMovie *movie, int stream, void *buffer, AviFormat from, AviFormat to, int *size);

int avi_get_data_id(AviFormat format, int stream);
int avi_get_format_type(AviFormat format);
int avi_get_format_fcc(AviFormat format);
int avi_get_format_compression(AviFormat format);

#endif

