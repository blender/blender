/*
 * render_int.h
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

#ifndef RENDER_INT_H
#define RENDER_INT_H

#include "zbuf_types.h"
#include "render_types.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*  float   CookTorr(float *n, float *l, float *v, int hard); */
void    do_lamphalo_tex(LampRen *lar, float *p1, float *p2, float *intens);
void    spothalo(struct LampRen *lar, float *view, float *intens);
void    add_filt_mask(unsigned int mask, unsigned short *col, unsigned int *rb1, unsigned int *rb2, unsigned int *rb3);
void    addps(long *rd, int vlak, unsigned int z, short ronde);
PixStr *addpsmain(void);
float   count_maskf(unsigned short mask);
void    freeps(void);
void    halovert(void);
void    renderhalo(HaloRen *har);	/* postprocess versie */
void scanlinehaloPS(unsigned int *rectz, long *rectdelta, unsigned int *rectt, short ys);

#endif /* RENDER_INT_H */

