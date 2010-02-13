/*
 * IMB_tiff.h
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
 * Contributor(s): Jonathan Merritt.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
/**
 * \file IMB_tiff.h
 * \ingroup imbuf
 * \brief Function declarations for tiff.c
 */

#ifndef IMB_TIFF_H
#define IMB_TIFF_H

/* Foward declaration of ImBuf structure. */
struct ImBuf;

/* Declarations for tiff.c */
int           imb_is_a_tiff(void *buf);
struct ImBuf *imb_loadtiff(unsigned char *mem, int size, int flags);
short         imb_savetiff(struct ImBuf *ibuf, char *name, int flags);
void*         libtiff_findsymbol(char *name);

#endif /* IMB_TIFF_H */

