/*
 * IMB_dpxcineon.h
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
/**
 * \file IMB_dpxcineon.h
 * \ingroup imbuf
 */
#ifndef _IMB_DPX_CINEON_H
#define _IMB_DPX_CINEON_H

struct ImBuf;

short imb_savecineon(struct ImBuf *buf, char *myfil, int flags);
struct ImBuf *imb_loadcineon(unsigned char *mem, int size, int flags);
int imb_is_cineon(void *buf);
short imb_save_dpx(struct ImBuf *buf, char *myfile, int flags);
struct ImBuf *imb_loaddpx(unsigned char *mem, int size, int flags);
int imb_is_dpx(void *buf);

#endif /*_IMB_DPX_CINEON_H*/
