/*
 * render_int.h
 * misc internal defines for renderer
 *
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

	/* XXX, should die, no good reason to write 
	 * regular (non-file related) endian dependant
	 * code.
	 */
#if defined(__sgi) || defined(__sparc) || defined(__sparc__) || defined (__PPC__) || defined (__ppc__) || defined (__BIG_ENDIAN__)
#define RCOMP	3
#define GCOMP	2
#define BCOMP	1
#define ACOMP	0
#else
#define RCOMP	0
#define GCOMP	1
#define BCOMP	2
#define ACOMP	3
#endif

#define VECCOPY(v1,v2) 		{*(v1)= *(v2); *(v1+1)= *(v2+1); *(v1+2)= *(v2+2);}
#define QUATCOPY(v1,v2) 	{*(v1)= *(v2); *(v1+1)= *(v2+1); *(v1+2)= *(v2+2); *(v1+3)= *(v2+3);}

#define CLAMPIS(a, b, c)	((a)<(b) ? (b) : (a)>(c) ? (c) : (a))

typedef struct { 
	int a, b, c, d;
} byte16;

#define COPY_16(a,b)		(*((byte16 *)(a))= *((byte16 *)(b)))

