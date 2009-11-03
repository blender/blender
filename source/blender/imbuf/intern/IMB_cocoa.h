/*
 * IMB_cocoa.h
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
 * Contributor(s): Damien Plisson 10/2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */
/**
 * \file IMB_cocoa.h
 * \ingroup imbuf
 * \brief Function declarations for imbuf_cocoa.m
 */

#ifndef IMB_COCOA_H
#define IMB_COCOA_H

/* Foward declaration of ImBuf structure. */
struct ImBuf;

/* Declarations for imbuf_cocoa.m */
struct ImBuf *imb_cocoaLoadImage(unsigned char *mem, int size, int flags);
short imb_cocoaSaveImage(struct ImBuf *ibuf, char *name, int flags);

#endif /* IMB_COCOA_H */

